#include "AEWArenaTools.h"
#include "AEWArenaMakerSettings.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Engine/Level.h"
#include "Engine/Selection.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Math/RandomStream.h"
#include "Misc/MessageDialog.h"
#include "Misc/PackageName.h"
#include "ScopedTransaction.h"
#include "StaticMeshResources.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "EditorStyleSet.h"
#include "Widgets/SWindow.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Components/LineBatchComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Editor/GroupActor.h"
#include "Containers/Ticker.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "UObject/StrongObjectPtr.h"

#define LOCTEXT_NAMESPACE "AEWArenaAudience"

DEFINE_LOG_CATEGORY_STATIC(LogAEWAudience, Log, All);

namespace
{
	const FName GeneratedTag(TEXT("AEWGeneratedAudience"));
	const FString SourcePrefix(TEXT("AEWAudienceFrom:"));

	struct FCrowdTemplate
	{
		TWeakObjectPtr<AActor> Actor;
		float YawOffset = 0.f;
		float ZOffset = 0.f;
	};

	struct FUpTriangle
	{
		FVector A;
		FVector B;
		FVector C;
	};

	bool IsGenerated(const AActor* A)
	{
		return A && A->Tags.Contains(GeneratedTag);
	}

	bool IsPerson(const AActor* A)
	{
		const FString Name = A ? A->GetClass()->GetName() : FString();
		return Name.StartsWith(TEXT("BP_Audience")) && !Name.StartsWith(TEXT("BP_AudienceManager"));
	}

	bool IsBillboard(const AActor* A)
	{
		const AStaticMeshActor* SM = Cast<AStaticMeshActor>(A);
		const UStaticMesh* Mesh = SM && SM->GetStaticMeshComponent() ? SM->GetStaticMeshComponent()->GetStaticMesh() : nullptr;
		return Mesh && Mesh->GetName().Contains(TEXT("audience_Billboard"));
	}

	bool IsCrowdManager(const AActor* A)
	{
		return A && A->GetClass()->GetName().StartsWith(TEXT("BP_AudienceManager"));
	}

	float YawTowards(const FVector& From, const FVector& To)
	{
		const FVector D = (To - From).GetSafeNormal2D();
		return D.IsNearlyZero() ? 0.f : D.Rotation().Yaw;
	}

	FName SourceTag(const FString& Label)
	{
		return FName(*(SourcePrefix + Label));
	}


	struct FAreaMarker
	{
		TWeakObjectPtr<AActor> Actor;
		FString Label;
		FVector Location = FVector::ZeroVector;
		float Yaw = 0.f;
	};

	bool MakeAreaMarker(AActor* A, FAreaMarker& Out)
	{
		if (!A || A->GetClass() != AActor::StaticClass() || !A->GetRootComponent())
		{
			return false;
		}
		TInlineComponentArray<UPrimitiveComponent*> Prims(A);
		for (UPrimitiveComponent* P : Prims)
		{
			if (!P->IsEditorOnly() && !P->IsVisualizationComponent())
			{
				return false;
			}
		}
		Out.Actor = A;
		Out.Label = A->GetActorLabel();
		Out.Location = A->GetActorLocation();
		Out.Yaw = A->GetActorRotation().Yaw;
		return true;
	}

	struct FBlockFrame
	{
		FVector Origin = FVector::ZeroVector;
		FVector Across = FVector::ZeroVector;
		FVector Depth = FVector::ZeroVector;
		FVector Facing = FVector::ForwardVector;
		float Curve = 0.f;

		bool IsCurved() const
		{
			return FMath::Abs(Curve) > 0.5f;
		}

		FVector Point(float A, float D, float& OutTurn) const
		{
			const FVector Center = Origin + Across * 0.5f + Depth * 0.5f;
			const float Width = FMath::Max(Across.Size(), 1.f);
			const FVector AcrossDir = Across / Width;
			const FVector DepthDir = Depth.GetSafeNormal();
			if (!IsCurved())
			{
				OutTurn = 0.f;
				return Center + AcrossDir * A + DepthDir * D;
			}
			const float Radius = Width / FMath::DegreesToRadians(Curve);
			const FVector Pivot = Center + Facing * Radius;
			const float Angle = A / Radius;
			OutTurn = FMath::RadiansToDegrees(Angle);
			const FVector Dir = Facing.RotateAngleAxis(OutTurn, FVector::UpVector);
			return Pivot - Dir * (Radius - D);
		}

		void Outline(float Z, TArray<FVector>& Out) const
		{
			Out.Reset();
			const float HalfA = Across.Size() * 0.5f;
			const float HalfD = Depth.Size() * 0.5f;
			const int32 Steps = IsCurved() ? 24 : 1;
			float Turn = 0.f;
			for (int32 i = 0; i <= Steps; ++i)
			{
				Out.Add(Point(-HalfA + 2.f * HalfA * i / Steps, HalfD, Turn));
			}
			for (int32 i = 0; i <= Steps; ++i)
			{
				Out.Add(Point(HalfA - 2.f * HalfA * i / Steps, -HalfD, Turn));
			}
			for (FVector& P : Out)
			{
				P.Z = Z;
			}
		}

		bool Contains(const FVector& P) const
		{
			if (IsCurved())
			{
				TArray<FVector> Points;
				Outline(P.Z, Points);
				FBox B(Points);
				return B.ExpandBy(10.f).IsInside(FVector(P.X, P.Y, P.Z));
			}
			const FVector D = FVector(P.X, P.Y, 0.f) - FVector(Origin.X, Origin.Y, 0.f);
			const float A2 = Across.SizeSquared2D();
			const float D2 = Depth.SizeSquared2D();
			if (A2 < KINDA_SMALL_NUMBER || D2 < KINDA_SMALL_NUMBER)
			{
				return false;
			}
			const float U = FVector::DotProduct(D, FVector(Across.X, Across.Y, 0.f)) / A2;
			const float V = FVector::DotProduct(D, FVector(Depth.X, Depth.Y, 0.f)) / D2;
			return U >= -0.02f && U <= 1.02f && V >= -0.02f && V <= 1.02f;
		}

		FBox Box(float Z) const
		{
			if (IsCurved())
			{
				TArray<FVector> Points;
				Outline(Z, Points);
				return FBox(Points);
			}
			FBox B(ForceInit);
			B += FVector(Origin.X, Origin.Y, Z);
			B += FVector(Origin.X, Origin.Y, Z) + Across;
			B += FVector(Origin.X, Origin.Y, Z) + Depth;
			B += FVector(Origin.X, Origin.Y, Z) + Across + Depth;
			return B;
		}
	};

	FVector FindRing(UWorld* World)
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->GetClass()->GetName() == TEXT("BP_Ring_C"))
			{
				return It->GetActorLocation();
			}
		}
		return FVector::ZeroVector;
	}

	FCrowdTemplate MakeTemplate(AActor* Actor, const FVector& Ring)
	{
		FCrowdTemplate T;
		T.Actor = Actor;
		const FVector Loc = Actor->GetActorLocation();
		T.YawOffset = FRotator::NormalizeAxis(Actor->GetActorRotation().Yaw - YawTowards(Loc, Ring));
		const FBox Bounds = Actor->GetComponentsBoundingBox(true);
		T.ZOffset = Bounds.IsValid ? FMath::Clamp(Loc.Z - Bounds.Min.Z, -50.f, 300.f) : 0.f;
		return T;
	}

	void CollectTemplates(ULevel* Level, const FVector& Ring, TArray<FCrowdTemplate>& People, TArray<FCrowdTemplate>& Billboards)
	{
		if (!Level)
		{
			return;
		}
		for (AActor* A : Level->Actors)
		{
			if (!A || IsGenerated(A))
			{
				continue;
			}
			if (IsPerson(A))
			{
				People.Add(MakeTemplate(A, Ring));
			}
			else if (IsBillboard(A))
			{
				Billboards.Add(MakeTemplate(A, Ring));
			}
		}
	}

	ULevel* FindAudienceLevel(UWorld* World, ULevel* Fallback)
	{
		ULevel* Named = nullptr;
		for (ULevel* Level : World->GetLevels())
		{
			if (!Level)
			{
				continue;
			}
			const FString Name = Level->GetOutermost()->GetName();
			if (!Name.Contains(TEXT("Audience")))
			{
				continue;
			}
			for (AActor* A : Level->Actors)
			{
				if (IsCrowdManager(A))
				{
					return Level;
				}
			}
			Named = Named ? Named : Level;
		}
		return Named ? Named : Fallback;
	}

	void GatherUpTriangles(UStaticMeshComponent* SM, float MinNormalZ, TArray<FUpTriangle>& Out)
	{
		UStaticMesh* Mesh = SM ? SM->GetStaticMesh() : nullptr;
		FStaticMeshRenderData* Render = Mesh ? Mesh->GetRenderData() : nullptr;
		if (!Render || Render->LODResources.Num() == 0)
		{
			return;
		}
		const FStaticMeshLODResources& LOD = Render->LODResources[0];
		const FPositionVertexBuffer& Positions = LOD.VertexBuffers.PositionVertexBuffer;
		if (!Positions.GetVertexData() || Positions.GetNumVertices() == 0)
		{
			return;
		}
		TArray<uint32> Indices;
		LOD.IndexBuffer.GetCopy(Indices);
		if (Indices.Num() < 3)
		{
			return;
		}
		TArray<FTransform> Transforms;
		if (UInstancedStaticMeshComponent* ISM = Cast<UInstancedStaticMeshComponent>(SM))
		{
			for (int32 i = 0; i < ISM->GetInstanceCount(); ++i)
			{
				FTransform T;
				if (ISM->GetInstanceTransform(i, T, true))
				{
					Transforms.Add(T);
				}
			}
		}
		else
		{
			Transforms.Add(SM->GetComponentTransform());
		}
		const int32 NumVerts = Positions.GetNumVertices();
		for (const FTransform& T : Transforms)
		{
			for (int32 i = 0; i + 2 < Indices.Num(); i += 3)
			{
				const uint32 I0 = Indices[i];
				const uint32 I1 = Indices[i + 1];
				const uint32 I2 = Indices[i + 2];
				if (I0 >= (uint32)NumVerts || I1 >= (uint32)NumVerts || I2 >= (uint32)NumVerts)
				{
					continue;
				}
				FUpTriangle Tri;
				Tri.A = T.TransformPosition(Positions.VertexPosition(I0));
				Tri.B = T.TransformPosition(Positions.VertexPosition(I1));
				Tri.C = T.TransformPosition(Positions.VertexPosition(I2));
				const FVector N = FVector::CrossProduct(Tri.B - Tri.A, Tri.C - Tri.A);
				const float Len = N.Size();
				if (Len < KINDA_SMALL_NUMBER)
				{
					continue;
				}
				const float Nz = N.Z / Len;
				if (FMath::Abs(Nz) >= MinNormalZ)
				{
					Out.Add(Tri);
				}
			}
		}
	}

	bool HeightAt(const FUpTriangle& T, float X, float Y, float& OutZ)
	{
		const float D = (T.B.Y - T.C.Y) * (T.A.X - T.C.X) + (T.C.X - T.B.X) * (T.A.Y - T.C.Y);
		if (FMath::Abs(D) < KINDA_SMALL_NUMBER)
		{
			return false;
		}
		const float W1 = ((T.B.Y - T.C.Y) * (X - T.C.X) + (T.C.X - T.B.X) * (Y - T.C.Y)) / D;
		const float W2 = ((T.C.Y - T.A.Y) * (X - T.C.X) + (T.A.X - T.C.X) * (Y - T.C.Y)) / D;
		const float W3 = 1.f - W1 - W2;
		const float E = -1e-4f;
		if (W1 < E || W2 < E || W3 < E)
		{
			return false;
		}
		OutZ = W1 * T.A.Z + W2 * T.B.Z + W3 * T.C.Z;
		return true;
	}

	struct FTriGrid
	{
		float Cell = 100.f;
		FVector2D Origin = FVector2D::ZeroVector;
		TMap<FIntPoint, TArray<int32>> Cells;

		FIntPoint Key(float X, float Y) const
		{
			return FIntPoint(FMath::FloorToInt((X - Origin.X) / Cell), FMath::FloorToInt((Y - Origin.Y) / Cell));
		}

		void Build(const TArray<FUpTriangle>& Tris, float InCell)
		{
			Cell = FMath::Max(InCell, 10.f);
			Cells.Reset();
			for (int32 i = 0; i < Tris.Num(); ++i)
			{
				const FUpTriangle& T = Tris[i];
				const FIntPoint Lo = Key(FMath::Min3(T.A.X, T.B.X, T.C.X), FMath::Min3(T.A.Y, T.B.Y, T.C.Y));
				const FIntPoint Hi = Key(FMath::Max3(T.A.X, T.B.X, T.C.X), FMath::Max3(T.A.Y, T.B.Y, T.C.Y));
				if ((int64)(Hi.X - Lo.X + 1) * (Hi.Y - Lo.Y + 1) > 4096)
				{
					continue;
				}
				for (int32 X = Lo.X; X <= Hi.X; ++X)
				{
					for (int32 Y = Lo.Y; Y <= Hi.Y; ++Y)
					{
						Cells.FindOrAdd(FIntPoint(X, Y)).Add(i);
					}
				}
			}
		}

		void Heights(const TArray<FUpTriangle>& Tris, float X, float Y, TArray<float>& Out) const
		{
			Out.Reset();
			const TArray<int32>* List = Cells.Find(Key(X, Y));
			if (!List)
			{
				return;
			}
			for (int32 Index : *List)
			{
				float Z = 0.f;
				if (HeightAt(Tris[Index], X, Y, Z))
				{
					Out.Add(Z);
				}
			}
			Out.Sort([](float A, float B) { return A > B; });
		}
	};

	void Toast(const FString& Message, bool bSuccess)
	{
		FNotificationInfo Info(FText::FromString(Message));
		Info.ExpireDuration = 7.f;
		Info.bUseLargeFont = false;
		if (TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info))
		{
			Item->SetCompletionState(bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
		}
	}

	bool CanStandOn(AActor* A)
	{
		if (!A || A->IsEditorOnly() || IsPerson(A) || IsBillboard(A) || IsCrowdManager(A) || IsGenerated(A))
		{
			return false;
		}
		const FString ClassName = A->GetClass()->GetName();
		if (ClassName.Contains(TEXT("Camera")) || ClassName.Contains(TEXT("Sky")) || ClassName.Contains(TEXT("Volume")))
		{
			return false;
		}
		TInlineComponentArray<UStaticMeshComponent*> Meshes(A);
		for (UStaticMeshComponent* SM : Meshes)
		{
			if (SM->GetStaticMesh() && !SM->IsEditorOnly() && !SM->IsVisualizationComponent() && !SM->bHiddenInGame && SM->IsVisible())
			{
				return true;
			}
		}
		return false;
	}

	struct FSurfaceData
	{
		TWeakObjectPtr<AActor> Actor;
		FString Label;
		TArray<UStaticMeshComponent*> Meshes;
		TArray<FUpTriangle> Tris;
		FTriGrid Grid;
		FBox Box = FBox(ForceInit);
		bool bUseTraces = false;

		bool Build(AActor* InActor, float MinNormalZ, float Cell)
		{
			Actor = InActor;
			Label = InActor->GetActorLabel();
			TInlineComponentArray<UStaticMeshComponent*> All(InActor);
			for (UStaticMeshComponent* SM : All)
			{
				if (SM->GetStaticMesh() && !SM->IsEditorOnly() && !SM->IsVisualizationComponent() && !SM->bHiddenInGame && SM->IsVisible())
				{
					Meshes.Add(SM);
					GatherUpTriangles(SM, MinNormalZ, Tris);
				}
			}
			bUseTraces = Tris.Num() == 0;
			if (bUseTraces)
			{
				Box = InActor->GetComponentsBoundingBox(true);
				return Box.IsValid != 0;
			}
			for (const FUpTriangle& T : Tris)
			{
				Box += T.A;
				Box += T.B;
				Box += T.C;
			}
			Grid.Origin = FVector2D(Box.Min.X, Box.Min.Y);
			Grid.Build(Tris, Cell);
			return true;
		}

		void Heights(float X, float Y, float MinNormalZ, TArray<float>& Out) const
		{
			Out.Reset();
			if (X < Box.Min.X || X > Box.Max.X || Y < Box.Min.Y || Y > Box.Max.Y)
			{
				return;
			}
			if (!bUseTraces)
			{
				Grid.Heights(Tris, X, Y, Out);
				return;
			}
			const FVector Start(X, Y, Box.Max.Z + 10.f);
			const FVector End(X, Y, Box.Min.Z - 10.f);
			for (UStaticMeshComponent* SM : Meshes)
			{
				FHitResult Hit;
				FCollisionQueryParams Query(NAME_None, true);
				if (SM->LineTraceComponent(Hit, Start, End, Query) && Hit.ImpactNormal.Z >= MinNormalZ)
				{
					Out.Add(Hit.ImpactPoint.Z);
				}
			}
			Out.Sort([](float A, float B) { return A > B; });
		}
	};

	int32 RemoveGeneratedIf(UWorld* World, TFunctionRef<bool(AActor*)> Match)
	{
		TArray<AActor*> Doomed;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (IsGenerated(*It) && Match(*It))
			{
				Doomed.Add(*It);
			}
		}
		for (AActor* A : Doomed)
		{
			A->Modify();
			World->EditorDestroyActor(A, true);
		}
		return Doomed.Num();
	}

	UClass* GroupActorClass()
	{
		static UClass* Class = FindObject<UClass>(ANY_PACKAGE, TEXT("GroupActor"));
		return Class;
	}

	bool IsGroup(const AActor* A)
	{
		UClass* Class = GroupActorClass();
		return A && Class && A->IsA(Class);
	}

	int32 RemoveEmptyCrowdGroups(UWorld* World)
	{
		TArray<AActor*> Empty;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (!IsGroup(*It) || !IsGenerated(*It))
			{
				continue;
			}
			TArray<AActor*> Members;
			static_cast<AGroupActor*>(*It)->GetGroupActors(Members);
			const bool bAnyLeft = Members.ContainsByPredicate([](AActor* M) { return IsValid(M); });
			if (!bAnyLeft)
			{
				Empty.Add(*It);
			}
		}
		for (AActor* A : Empty)
		{
			A->Modify();
			World->EditorDestroyActor(A, true);
		}
		return Empty.Num();
	}


	struct FFillOptions
	{
		float Fill = 0.85f;
		int32 PerRow = 10;
		int32 Rows = 5;
		float Seat = 70.f;
		float Row = 90.f;
		float Straight = 0.8f;
		float Billboard = 0.f;
		float Curve = 0.f;
		EAEWCrowdType Type = EAEWCrowdType::ByDistance;
	};

	struct FSpot
	{
		FString Area;
		FString Surface;
		FVector Location;
		float Yaw = 0.f;
		bool bBillboard = false;
		int32 Template = 0;
	};

	struct FArea
	{
		FString Label;
		int32 Marker = INDEX_NONE;
		TArray<TSharedPtr<FSurfaceData>> Under;
		TArray<FVector> Outline;
	};

	struct FFillContext
	{
		TWeakObjectPtr<UWorld> World;
		TArray<FAreaMarker> Markers;
		TArray<TSharedPtr<FSurfaceData>> Surfaces;
		bool bExplicitSurfaces = false;
		TMap<TWeakObjectPtr<AActor>, TSharedPtr<FSurfaceData>> SurfaceCache;
		TArray<FArea> Areas;
		TArray<FCrowdTemplate> People;
		TArray<FCrowdTemplate> Billboards;
		TStrongObjectPtr<UWorld> TemplateWorld;
		FString TemplateSource;
		FVector Ring = FVector::ZeroVector;
		float MinNormalZ = 0.86f;
		FFillOptions Options;
		TArray<FSpot> Spots;
		TArray<FString> Report;
		int32 PeopleCount = 0;
		int32 BillboardCount = 0;
		bool bDirty = true;
		bool bDone = false;
		TStrongObjectPtr<ULineBatchComponent> Preview;
		TArray<TSharedPtr<TStrongObjectPtr<USkeletalMeshComponent>>> PersonPool;
		TArray<int32> PoolTemplate;
		TMap<int32, TSharedPtr<TStrongObjectPtr<UInstancedStaticMeshComponent>>> BillboardPool;
		TWeakPtr<SWindow> Window;
		FDelegateHandle Ticker;
		FDelegateHandle Cleanup;
	};

	TWeakPtr<SWindow> ActiveFillWindow;

	const FString AreaPrefix(TEXT("AEWAudienceArea:"));

	FName AreaTag(const FString& Label)
	{
		return FName(*(AreaPrefix + Label));
	}

	FBlockFrame MarkerFrame(const FAreaMarker& M, const FFillOptions& O)
	{
		FBlockFrame F;
		F.Facing = FRotator(0.f, M.Yaw, 0.f).Vector();
		const FVector Right(-F.Facing.Y, F.Facing.X, 0.f);
		const float Width = (O.Seat > 0.f ? O.Seat : 70.f) * FMath::Max(O.PerRow, 1);
		const float DepthLength = (O.Row > 0.f ? O.Row : 90.f) * FMath::Max(O.Rows, 1);
		F.Across = Right * Width;
		F.Depth = F.Facing * DepthLength;
		F.Origin = M.Location - F.Across * 0.5f - F.Depth * 0.5f;
		F.Curve = O.Curve;
		return F;
	}

	FBlockFrame SurfaceFrame(const FFillContext& Ctx, const FArea& Area)
	{
		FVector Center = FVector::ZeroVector;
		for (const FVector& P : Area.Outline)
		{
			Center += P;
		}
		Center /= FMath::Max(1, Area.Outline.Num());
		FVector Forward = (Ctx.Ring - Center).GetSafeNormal2D();
		if (Forward.IsNearlyZero())
		{
			Forward = FVector::ForwardVector;
		}
		const FVector Right(-Forward.Y, Forward.X, 0.f);
		float MinF = TNumericLimits<float>::Max();
		float MaxF = TNumericLimits<float>::Lowest();
		float MinR = TNumericLimits<float>::Max();
		float MaxR = TNumericLimits<float>::Lowest();
		auto Extend = [&](const FVector& P)
		{
			MinF = FMath::Min(MinF, FVector::DotProduct(P, Forward));
			MaxF = FMath::Max(MaxF, FVector::DotProduct(P, Forward));
			MinR = FMath::Min(MinR, FVector::DotProduct(P, Right));
			MaxR = FMath::Max(MaxR, FVector::DotProduct(P, Right));
		};
		for (const FVector& P : Area.Outline)
		{
			Extend(P);
		}
		if (Area.Under.Num() > 0)
		{
			for (const FUpTriangle& T : Area.Under[0]->Tris)
			{
				Extend(T.A);
				Extend(T.B);
				Extend(T.C);
			}
		}
		FBlockFrame F;
		F.Origin = Forward * MinF + Right * MinR;
		F.Depth = Forward * FMath::Max(MaxF - MinF, 1.f);
		F.Across = Right * FMath::Max(MaxR - MinR, 1.f);
		F.Facing = Forward;
		F.Curve = Ctx.Options.Curve;
		return F;
	}

	FBlockFrame AreaFrame(const FFillContext& Ctx, const FArea& Area)
	{
		return Area.Marker != INDEX_NONE ? MarkerFrame(Ctx.Markers[Area.Marker], Ctx.Options) : SurfaceFrame(Ctx, Area);
	}

	TArray<TSharedPtr<FSurfaceData>> SurfacesUnder(FFillContext& Ctx, const FBlockFrame& Frame, float TopZ)
	{
		if (Ctx.bExplicitSurfaces)
		{
			return Ctx.Surfaces;
		}
		TArray<TSharedPtr<FSurfaceData>> Out;
		UWorld* World = Ctx.World.Get();
		if (!World)
		{
			return Out;
		}
		const FBox Area = Frame.Box(TopZ).ExpandBy(FVector(50.f, 50.f, 0.f));
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* A = *It;
			const FBox B = A->GetComponentsBoundingBox(true);
			if (!B.IsValid || B.Min.X > Area.Max.X || B.Max.X < Area.Min.X || B.Min.Y > Area.Max.Y || B.Max.Y < Area.Min.Y || B.Min.Z > TopZ + 100.f)
			{
				continue;
			}
			if (!CanStandOn(A))
			{
				continue;
			}
			TSharedPtr<FSurfaceData>& Cached = Ctx.SurfaceCache.FindOrAdd(A);
			if (!Cached.IsValid())
			{
				Cached = MakeShared<FSurfaceData>();
				if (!Cached->Build(A, Ctx.MinNormalZ, 100.f))
				{
					continue;
				}
			}
			if (Cached->Actor.IsValid())
			{
				Out.Add(Cached);
			}
		}
		return Out;
	}

	bool InsideAnyMarker(const FFillContext& Ctx, const AActor* A)
	{
		for (const FAreaMarker& M : Ctx.Markers)
		{
			if (A->Tags.Contains(AreaTag(M.Label)) || MarkerFrame(M, Ctx.Options).Contains(A->GetActorLocation()))
			{
				return true;
			}
		}
		return false;
	}

	void ComputeSpots(FFillContext& Ctx)
	{
		const FFillOptions& O = Ctx.Options;
		const int32 PerRow = FMath::Clamp(O.PerRow, 1, 2000);
		const int32 Rows = FMath::Clamp(O.Rows, 1, 2000);
		const float Fill = FMath::Clamp(O.Fill, 0.f, 1.f);
		const float Scatter = 1.f - FMath::Clamp(O.Straight, 0.f, 1.f);
		const float BillboardDistance = Ctx.Billboards.Num() > 0 ? O.Billboard : 0.f;
		const bool bAllBillboards = O.Type == EAEWCrowdType::Billboards && Ctx.Billboards.Num() > 0;
		const bool bAllPeople = O.Type == EAEWCrowdType::People || (O.Type == EAEWCrowdType::Billboards && Ctx.Billboards.Num() == 0);

		Ctx.Spots.Reset();
		Ctx.Report.Reset();
		Ctx.PeopleCount = 0;
		Ctx.BillboardCount = 0;
		TArray<float> Heights;
		for (FArea& Area : Ctx.Areas)
		{
			const FAreaMarker* Marker = Area.Marker != INDEX_NONE ? &Ctx.Markers[Area.Marker] : nullptr;
			const FBlockFrame Frame = AreaFrame(Ctx, Area);
			if (Marker)
			{
				Area.Under = SurfacesUnder(Ctx, Frame, Marker->Location.Z);
			}
			const float AreaYaw = Frame.Facing.Rotation().Yaw;
			const float AcrossLength = FMath::Max(Frame.Across.Size(), 1.f);
			const float DepthLength = FMath::Max(Frame.Depth.Size(), 1.f);
			const float AcrossStep = (!Marker && O.Seat <= 0.f) ? AcrossLength / PerRow : (O.Seat > 0.f ? O.Seat : 70.f);
			const float DepthStep = (!Marker && O.Row <= 0.f) ? DepthLength / Rows : (O.Row > 0.f ? O.Row : 90.f);
			const float AcrossStart = AcrossLength * 0.5f - AcrossStep * (PerRow - 1) * 0.5f;
			const float DepthStart = DepthLength * 0.5f - DepthStep * (Rows - 1) * 0.5f;

			const int32 Total = PerRow * Rows;
			if (Total > 200000)
			{
				Ctx.Report.Add(FString::Printf(TEXT("%s: %d x %d is too many spots, skipped"), *Area.Label, PerRow, Rows));
				continue;
			}
			const int32 Wanted = FMath::RoundToInt(Total * Fill);
			const uint32 Seed = GetTypeHash(Area.Label);
			TArray<int32> Order;
			Order.Reserve(Total);
			for (int32 i = 0; i < Total; ++i)
			{
				Order.Add(i);
			}
			FRandomStream Shuffle(Seed);
			for (int32 i = Total - 1; i > 0; --i)
			{
				Order.Swap(i, Shuffle.RandRange(0, i));
			}
			Order.SetNum(Wanted);
			Order.Sort();

			int32 Placed = 0;
			int32 InAir = 0;
			int32 Missed = 0;
			for (const int32 Cell : Order)
			{
				const int32 Column = Cell % PerRow;
				const int32 RowIndex = Cell / PerRow;
				FRandomStream Random(HashCombine(Seed, GetTypeHash(Cell)));
				const float AcrossPos = AcrossStart + (Column + Random.FRandRange(-0.5f, 0.5f) * Scatter) * AcrossStep;
				const float DepthPos = DepthStart + (RowIndex + Random.FRandRange(-0.5f, 0.5f) * Scatter) * DepthStep;
				const int32 Pick = Random.RandHelper(1 << 20);
				float CurveTurn = 0.f;
				const FVector XY = Frame.Point(AcrossPos - AcrossLength * 0.5f, DepthPos - DepthLength * 0.5f, CurveTurn);

				const float Limit = Marker ? Marker->Location.Z + 100.f : TNumericLimits<float>::Max();
				bool bFound = false;
				float BestZ = 0.f;
				FString BestSurface;
				for (const TSharedPtr<FSurfaceData>& D : Area.Under)
				{
					if (!D->Actor.IsValid())
					{
						continue;
					}
					D->Heights(XY.X, XY.Y, Ctx.MinNormalZ, Heights);
					for (float Z : Heights)
					{
						if (Z <= Limit && (!bFound || Z > BestZ))
						{
							BestZ = Z;
							BestSurface = D->Label;
							bFound = true;
						}
					}
				}
				if (!bFound && Marker)
				{
					BestZ = Marker->Location.Z;
					BestSurface = Area.Label;
					bFound = true;
					++InAir;
				}
				if (!bFound)
				{
					++Missed;
					continue;
				}
				FSpot Spot;
				Spot.Area = Area.Label;
				Spot.Surface = BestSurface;
				Spot.Location = FVector(XY.X, XY.Y, BestZ);
				Spot.bBillboard = bAllBillboards || (!bAllPeople && BillboardDistance > 0.f && FVector::Dist2D(Spot.Location, Ctx.Ring) > BillboardDistance);
				Spot.Template = Pick % (Spot.bBillboard ? Ctx.Billboards.Num() : Ctx.People.Num());
				Spot.Yaw = AreaYaw + CurveTurn;
				Ctx.Spots.Add(Spot);
				(Spot.bBillboard ? Ctx.BillboardCount : Ctx.PeopleCount)++;
				++Placed;
			}
			FString Line = FString::Printf(TEXT("%s: %d x %d = %d spots, %d placed"), *Area.Label, PerRow, Rows, Total, Placed);
			if (InAir > 0)
			{
				Line += FString::Printf(TEXT(", %d at the marker's height (no surface under them)"), InAir);
			}
			if (Missed > 0)
			{
				Line += FString::Printf(TEXT(", %d with no surface"), Missed);
			}
			Ctx.Report.Add(Line);
		}
	}


	FTransform SpotTransform(const FCrowdTemplate& T, const AActor* Source, const FSpot& Spot)
	{
		const FRotator Rotation(Spot.bBillboard ? Source->GetActorRotation().Pitch : 0.f, Spot.Yaw + T.YawOffset, Spot.bBillboard ? Source->GetActorRotation().Roll : 0.f);
		return FTransform(Rotation, Spot.Location + FVector(0.f, 0.f, T.ZOffset), Source->GetActorScale3D());
	}

	TMap<TPair<UClass*, UClass*>, FTransform> DefaultLocalCache;

	FTransform DefaultComponentLocal(UClass* ActorClass, UClass* ComponentClass)
	{
		const TPair<UClass*, UClass*> Key(ActorClass, ComponentClass);
		if (const FTransform* Found = DefaultLocalCache.Find(Key))
		{
			return *Found;
		}
		FTransform Result = FTransform::Identity;
		bool bFound = false;
		if (const AActor* CDO = ActorClass ? ActorClass->GetDefaultObject<AActor>() : nullptr)
		{
			TInlineComponentArray<USceneComponent*> Comps;
			CDO->GetComponents(Comps);
			for (USceneComponent* C : Comps)
			{
				if (C->IsA(ComponentClass))
				{
					Result = C == CDO->GetRootComponent() ? FTransform::Identity : C->GetRelativeTransform();
					bFound = true;
					break;
				}
			}
		}
		for (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(ActorClass); !bFound && BPGC; BPGC = Cast<UBlueprintGeneratedClass>(BPGC->GetSuperClass()))
		{
			USimpleConstructionScript* SCS = BPGC->SimpleConstructionScript;
			if (!SCS)
			{
				continue;
			}
			for (USCS_Node* Node : SCS->GetAllNodes())
			{
				USceneComponent* Template = Node ? Cast<USceneComponent>(Node->ComponentTemplate) : nullptr;
				if (Template && Template->IsA(ComponentClass))
				{
					Result = SCS->GetRootNodes().Contains(Node) ? FTransform::Identity : Template->GetRelativeTransform();
					bFound = true;
					break;
				}
			}
		}
		DefaultLocalCache.Add(Key, Result);
		return Result;
	}

	void CopyMaterials(UMeshComponent* From, UMeshComponent* To)
	{
		const int32 Count = From->GetNumMaterials();
		for (int32 i = 0; i < Count; ++i)
		{
			To->SetMaterial(i, From->GetMaterial(i));
		}
	}

	template <typename TComp>
	TComp* NewPreviewComponent(UWorld* World)
	{
		TComp* C = NewObject<TComp>(GetTransientPackage(), NAME_None, RF_Transient);
		C->SetMobility(EComponentMobility::Movable);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		C->bSelectable = false;
		C->SetComponentTickEnabled(false);
		C->RegisterComponentWithWorld(World);
		return C;
	}

	void DrawPreview(FFillContext& Ctx)
	{
		UWorld* World = Ctx.World.Get();
		if (!World)
		{
			return;
		}
		if (!Ctx.Preview.IsValid())
		{
			ULineBatchComponent* NewLines = NewObject<ULineBatchComponent>(GetTransientPackage(), NAME_None, RF_Transient);
			NewLines->bCalculateAccurateBounds = false;
			NewLines->RegisterComponentWithWorld(World);
			Ctx.Preview.Reset(NewLines);
		}
		ULineBatchComponent* Lines = Ctx.Preview.Get();
		Lines->Flush();
		TArray<FBatchedLine> Batch;
		for (const FAreaMarker& M : Ctx.Markers)
		{
			const FBlockFrame F = MarkerFrame(M, Ctx.Options);
			TArray<FVector> Corners;
			F.Outline(M.Location.Z, Corners);
			for (int32 i = 0; i < Corners.Num(); ++i)
			{
				Batch.Add(FBatchedLine(Corners[i], Corners[(i + 1) % Corners.Num()], FLinearColor(0.f, 0.8f, 1.f), 0.f, 4.f, SDPG_Foreground));
			}
			const FVector Front = M.Location + F.Facing * (F.Depth.Size() * 0.5f + 60.f);
			Batch.Add(FBatchedLine(M.Location, Front, FLinearColor(0.f, 0.8f, 1.f), 0.f, 6.f, SDPG_Foreground));
		}

		const int32 MaxPeopleModels = 1500;
		int32 PersonIndex = 0;
		TMap<int32, TArray<FTransform>> BillboardInstances;
		for (const FSpot& Spot : Ctx.Spots)
		{
			const FCrowdTemplate& T = Spot.bBillboard ? Ctx.Billboards[Spot.Template] : Ctx.People[Spot.Template];
			const AActor* Source = T.Actor.Get();
			USkeletalMeshComponent* SourceSkel = Source && !Spot.bBillboard ? Source->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
			UStaticMeshComponent* SourceStatic = Source && Spot.bBillboard ? Source->FindComponentByClass<UStaticMeshComponent>() : nullptr;
			const bool bModel = Spot.bBillboard ? (SourceStatic && SourceStatic->GetStaticMesh()) : (SourceSkel && SourceSkel->SkeletalMesh && PersonIndex < MaxPeopleModels);
			if (!bModel)
			{
				const FLinearColor Color = Spot.bBillboard ? FLinearColor(1.f, 0.55f, 0.f) : FLinearColor(0.2f, 1.f, 0.3f);
				const FVector Facing = FRotator(0.f, Spot.Yaw, 0.f).Vector() * 35.f;
				const FVector Chest = Spot.Location + FVector(0.f, 0.f, 120.f);
				Batch.Add(FBatchedLine(Spot.Location, Spot.Location + FVector(0.f, 0.f, 170.f), Color, 0.f, 3.f, SDPG_World));
				Batch.Add(FBatchedLine(Chest, Chest + Facing, Color, 0.f, 3.f, SDPG_World));
				continue;
			}
			const FTransform ActorTransform = SpotTransform(T, Source, Spot);
			if (Spot.bBillboard)
			{
				const FTransform Local = SourceStatic->GetComponentTransform().GetRelativeTransform(Source->GetActorTransform());

				BillboardInstances.FindOrAdd(Spot.Template).Add(Local * ActorTransform);
				continue;
			}
			if (Ctx.PersonPool.Num() <= PersonIndex)
			{
				Ctx.PersonPool.Add(MakeShared<TStrongObjectPtr<USkeletalMeshComponent>>(NewPreviewComponent<USkeletalMeshComponent>(World)));
				Ctx.PoolTemplate.Add(INDEX_NONE);
			}
			USkeletalMeshComponent* Comp = Ctx.PersonPool[PersonIndex]->Get();
			if (Ctx.PoolTemplate[PersonIndex] != Spot.Template)
			{
				Comp->bNoSkeletonUpdate = false;
				Comp->SetSkeletalMesh(SourceSkel->SkeletalMesh);
				CopyMaterials(SourceSkel, Comp);
				Comp->SetRelativeScale3D(FVector::OneVector);
				Ctx.PoolTemplate[PersonIndex] = Spot.Template;
			}
			const FTransform Local = DefaultComponentLocal(Source->GetClass(), USkeletalMeshComponent::StaticClass());
			Comp->SetWorldTransform(Local * ActorTransform);
			Comp->SetVisibility(true);
			++PersonIndex;
		}
		for (int32 i = PersonIndex; i < Ctx.PersonPool.Num(); ++i)
		{
			Ctx.PersonPool[i]->Get()->SetVisibility(false);
		}

		for (TPair<int32, TSharedPtr<TStrongObjectPtr<UInstancedStaticMeshComponent>>>& Pair : Ctx.BillboardPool)
		{
			Pair.Value->Get()->ClearInstances();
		}
		for (TPair<int32, TArray<FTransform>>& Pair : BillboardInstances)
		{
			TSharedPtr<TStrongObjectPtr<UInstancedStaticMeshComponent>>& Slot = Ctx.BillboardPool.FindOrAdd(Pair.Key);
			if (!Slot.IsValid())
			{
				const AActor* Source = Ctx.Billboards[Pair.Key].Actor.Get();
				UStaticMeshComponent* SourceStatic = Source ? Source->FindComponentByClass<UStaticMeshComponent>() : nullptr;
				if (!SourceStatic)
				{
					continue;
				}
				UInstancedStaticMeshComponent* ISM = NewPreviewComponent<UInstancedStaticMeshComponent>(World);
				ISM->SetStaticMesh(SourceStatic->GetStaticMesh());
				CopyMaterials(SourceStatic, ISM);
				Slot = MakeShared<TStrongObjectPtr<UInstancedStaticMeshComponent>>(ISM);
			}
			Slot->Get()->AddInstances(Pair.Value, false);
		}

		if (Batch.Num() > 0)
		{
			Lines->DrawLines(Batch);
		}
		if (GEditor)
		{
			GEditor->RedrawLevelEditingViewports(true);
		}
	}

	void DestroyPreviewComponent(UActorComponent* C)
	{
		if (C)
		{
			if (C->IsRegistered())
			{
				C->UnregisterComponent();
			}
			C->DestroyComponent();
		}
	}

	void EndPreview(FFillContext& Ctx)
	{
		if (Ctx.Cleanup.IsValid())
		{
			FWorldDelegates::OnWorldCleanup.Remove(Ctx.Cleanup);
			Ctx.Cleanup.Reset();
		}
		if (Ctx.Ticker.IsValid())
		{
			FTicker::GetCoreTicker().RemoveTicker(Ctx.Ticker);
			Ctx.Ticker.Reset();
		}
		if (Ctx.Preview.IsValid())
		{
			Ctx.Preview->Flush();
			DestroyPreviewComponent(Ctx.Preview.Get());
			Ctx.Preview.Reset();
		}
		for (TSharedPtr<TStrongObjectPtr<USkeletalMeshComponent>>& C : Ctx.PersonPool)
		{
			DestroyPreviewComponent(C->Get());
		}
		Ctx.PersonPool.Reset();
		Ctx.PoolTemplate.Reset();
		for (TPair<int32, TSharedPtr<TStrongObjectPtr<UInstancedStaticMeshComponent>>>& Pair : Ctx.BillboardPool)
		{
			DestroyPreviewComponent(Pair.Value->Get());
		}
		Ctx.BillboardPool.Reset();
		if (GEditor)
		{
			GEditor->RedrawLevelEditingViewports(true);
		}
	}

	void SaveOptions(const FFillOptions& O)
	{
		UAEWArenaMakerSettings* S = GetMutableDefault<UAEWArenaMakerSettings>();
		S->AudienceFill = FMath::Clamp(O.Fill, 0.f, 1.f);
		S->AudienceSeatSpacing = FMath::Max(O.Seat, 0.f);
		S->AudiencePerRow = FMath::Clamp(O.PerRow, 1, 2000);
		S->AudienceRows = FMath::Clamp(O.Rows, 1, 2000);
		S->AudienceRowCurve = FMath::Clamp(O.Curve, -270.f, 270.f);
		S->AudienceRowSpacing = FMath::Max(O.Row, 0.f);
		S->AudienceStraightness = FMath::Clamp(O.Straight, 0.f, 1.f);
		S->AudienceBillboardDistance = FMath::Max(O.Billboard, 0.f);
		S->AudienceCrowdType = O.Type;
		S->SaveConfig();
		S->UpdateDefaultConfigFile();
	}

	int32 SpawnCrowd(FFillContext& Ctx)
	{
		const FText Title = LOCTEXT("FillTitle", "Fill Audience From Selected");
		UWorld* World = Ctx.World.Get();
		if (!World || (GEditor && GEditor->PlayWorld))
		{
			return 0;
		}
		for (const FCrowdTemplate& T : Ctx.People)
		{
			if (!T.Actor.IsValid())
			{
				FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("TemplatesGone", "The crowd people to copy are no longer loaded. Run Fill Audience From Selected again."), &Title);
				return 0;
			}
		}
		if (Ctx.Spots.Num() == 0)
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Nowhere to stand the crowd:\n  ") + FString::Join(Ctx.Report, TEXT("\n  "))), &Title);
			return 0;
		}

		ULevel* Fallback = World->PersistentLevel;
		for (const TSharedPtr<FSurfaceData>& D : Ctx.Surfaces)
		{
			if (D->Actor.IsValid())
			{
				Fallback = D->Actor->GetLevel();
				break;
			}
		}
		ULevel* Target = FindAudienceLevel(World, Fallback);
		const FScopedTransaction Transaction(Title);
		Target->Modify();

		int32 Replaced = 0;
		if (Ctx.Markers.Num() > 0)
		{
			Replaced = RemoveGeneratedIf(World, [&Ctx](AActor* A) { return InsideAnyMarker(Ctx, A); });
		}
		else
		{
			TSet<FName> Sources;
			for (const TSharedPtr<FSurfaceData>& D : Ctx.Surfaces)
			{
				Sources.Add(SourceTag(D->Label));
			}
			Replaced = RemoveGeneratedIf(World, [&Sources](AActor* A) { return A->Tags.ContainsByPredicate([&Sources](const FName& T) { return Sources.Contains(T); }); });
		}

		EditorUtilities::FCopyOptions Copy(EditorUtilities::ECopyOptions::Default);
		Copy.PropertyFilter = [](FProperty& Property, UObject&)
		{
			const FName Name = Property.GetFName();
			return Name != TEXT("ActorLabel") && Name != TEXT("FolderPath") && Name != TEXT("RelativeLocation") && Name != TEXT("RelativeRotation") && Name != TEXT("AttachParent");
		};

		RemoveEmptyCrowdGroups(World);

		int32 Made = 0;
		TMap<FString, int32> PerArea;
		TMap<FString, TArray<AActor*>> Members;
		for (const FSpot& Spot : Ctx.Spots)
		{
			const FCrowdTemplate& T = Spot.bBillboard ? Ctx.Billboards[Spot.Template] : Ctx.People[Spot.Template];
			AActor* Source = T.Actor.Get();
			if (!Source)
			{
				continue;
			}
			const FTransform Where = SpotTransform(T, Source, Spot);
			const FVector Location = Where.GetLocation();
			const FRotator Rotation = Where.Rotator();
			FActorSpawnParameters Params;
			Params.OverrideLevel = Target;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AActor* New = World->SpawnActor<AActor>(Source->GetClass(), Location, Rotation, Params);
			if (!New)
			{
				continue;
			}
			New->Modify();
			EditorUtilities::CopyActorProperties(Source, New, Copy);
			New->SetActorLocationAndRotation(Location, Rotation);
			New->SetActorScale3D(Source->GetActorScale3D());
			New->Tags.AddUnique(GeneratedTag);
			New->Tags.AddUnique(SourceTag(Spot.Surface));
			New->Tags.AddUnique(AreaTag(Spot.Area));
			int32& Count = PerArea.FindOrAdd(Spot.Area);
			++Count;
			New->SetActorLabel(FString::Printf(TEXT("%s_%s_%03d"), Spot.bBillboard ? TEXT("CrowdBillboard") : TEXT("Crowd"), *Spot.Area, Count));
			New->SetFolderPath(FName(*(TEXT("Audience/Generated/") + Spot.Area)));
			New->PostEditChange();
			Members.FindOrAdd(Spot.Area).Add(New);
			++Made;
		}

		if (UClass* GroupClass = GroupActorClass())
		{
			for (TPair<FString, TArray<AActor*>>& Pair : Members)
			{
				if (Pair.Value.Num() == 0)
				{
					continue;
				}
				FActorSpawnParameters Params;
				Params.OverrideLevel = Target;
				Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				const FTransform At(Pair.Value[0]->GetActorLocation());
				AActor* GroupBase = World->SpawnActor(GroupClass, &At, Params);
				if (!GroupBase)
				{
					continue;
				}
				AGroupActor* Group = static_cast<AGroupActor*>(GroupBase);
				Group->Modify();
				for (AActor* Member : Pair.Value)
				{
					Group->Add(*Member);
				}
				Group->CenterGroupLocation();
				Group->Tags.AddUnique(GeneratedTag);
				Group->Tags.AddUnique(AreaTag(Pair.Key));
				Group->SetActorLabel(TEXT("CrowdGroup_") + Pair.Key);
				Group->SetFolderPath(FName(*(TEXT("Audience/Generated/") + Pair.Key)));
			}
		}
		Target->MarkPackageDirty();

		int32 MarkersRemoved = 0;
		for (const FAreaMarker& M : Ctx.Markers)
		{
			if (AActor* MarkerActor = M.Actor.Get())
			{
				MarkerActor->Modify();
				if (MarkerActor->GetLevel())
				{
					MarkerActor->GetLevel()->Modify();
				}
				World->EditorDestroyActor(MarkerActor, true);
				++MarkersRemoved;
			}
		}

		const FString TargetName = FPackageName::GetShortName(Target->GetOutermost()->GetName());
		for (const FString& Line : Ctx.Report)
		{
			UE_LOG(LogAEWAudience, Display, TEXT("%s"), *Line);
		}
		UE_LOG(LogAEWAudience, Display, TEXT("Placed %d crowd actor(s) in %s, copied from %s (%d people, %d billboards to pick from)%s%s."),
			Made, *TargetName, *Ctx.TemplateSource, Ctx.People.Num(), Ctx.Billboards.Num(),
			Replaced > 0 ? *FString::Printf(TEXT(", replacing %d"), Replaced) : TEXT(""),
			MarkersRemoved > 0 ? *FString::Printf(TEXT(", %d marker(s) removed"), MarkersRemoved) : TEXT(""));
		Toast(FString::Printf(TEXT("Placed %d crowd actor(s) in %s%s. Save the level to keep them."), Made, *TargetName,
			MarkersRemoved > 0 ? TEXT(" and removed the marker(s)") : TEXT("")), Made > 0);
		return Made;
	}

	void OpenFillWindow(TSharedRef<FFillContext> Ctx)
	{
		TSharedRef<SWindow> Window = SNew(SWindow)
			.Title(LOCTEXT("OptionsTitle", "Fill Audience"))
			.SizingRule(ESizingRule::Autosized)
			.SupportsMinimize(false)
			.SupportsMaximize(false);
		Ctx->Window = Window;
		ActiveFillWindow = Window;
		TWeakPtr<SWindow> WeakWindow = Window;

		TSharedRef<SGridPanel> Grid = SNew(SGridPanel).FillColumn(1, 1.f);
		int32 Line = 0;
		auto Radio = [Ctx](EAEWCrowdType Type, const FText& Text)
		{
			return SNew(SCheckBox)
				.Style(FEditorStyle::Get(), "RadioButton")
				.IsChecked_Lambda([Ctx, Type]() { return Ctx->Options.Type == Type ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([Ctx, Type](ECheckBoxState) { Ctx->Options.Type = Type; Ctx->bDirty = true; })
				[
					SNew(STextBlock).Text(Text)
				];
		};
		Grid->AddSlot(0, Line).Padding(0.f, 3.f, 12.f, 3.f).VAlign(VAlign_Center)
		[
			SNew(STextBlock).Text(LOCTEXT("OptType", "Type")).ToolTipText(LOCTEXT("OptTypeTip", "By distance: people near the ring, billboards beyond the distance below. People only / Billboards only: the whole fill is one kind."))
		];
		Grid->AddSlot(1, Line).Padding(0.f, 3.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 10.f, 0.f)[ Radio(EAEWCrowdType::ByDistance, LOCTEXT("TypeDistance", "Distance")) ]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 10.f, 0.f)[ Radio(EAEWCrowdType::People, LOCTEXT("TypePeople", "People")) ]
			+ SHorizontalBox::Slot().AutoWidth()[ Radio(EAEWCrowdType::Billboards, LOCTEXT("TypeBillboards", "Billboards")) ]
		];
		++Line;
		auto AddRow = [&Grid, &Line, Ctx](const FText& Label, const FText& Tip, float FFillOptions::* Member, float Scale, float Min, float Max, float Step, TAttribute<bool> Enabled = true)
		{
			Grid->AddSlot(0, Line).Padding(0.f, 3.f, 12.f, 3.f).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(Label).ToolTipText(Tip)
			];
			Grid->AddSlot(1, Line).Padding(0.f, 3.f)
			[
				SNew(SBox).MinDesiredWidth(140.f)
				[
					SNew(SSpinBox<float>)
					.IsEnabled(Enabled)
					.MinValue(Min).MaxValue(Max).Delta(Step)
					.ToolTipText(Tip)
					.Value_Lambda([Ctx, Member, Scale]() { return Ctx->Options.*Member * Scale; })
					.OnValueChanged_Lambda([Ctx, Member, Scale](float V) { Ctx->Options.*Member = V / Scale; Ctx->bDirty = true; })
					.OnValueCommitted_Lambda([Ctx, Member, Scale](float V, ETextCommit::Type) { Ctx->Options.*Member = V / Scale; Ctx->bDirty = true; })
				]
			];
			++Line;
		};
		auto AddCount = [&Grid, &Line, Ctx](const FText& Label, const FText& Tip, int32 FFillOptions::* Member)
		{
			Grid->AddSlot(0, Line).Padding(0.f, 3.f, 12.f, 3.f).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(Label).ToolTipText(Tip)
			];
			Grid->AddSlot(1, Line).Padding(0.f, 3.f)
			[
				SNew(SBox).MinDesiredWidth(140.f)
				[
					SNew(SSpinBox<int32>)
					.MinValue(1).MaxValue(2000).Delta(1)
					.ToolTipText(Tip)
					.Value_Lambda([Ctx, Member]() { return Ctx->Options.*Member; })
					.OnValueChanged_Lambda([Ctx, Member](int32 V) { Ctx->Options.*Member = FMath::Max(1, V); Ctx->bDirty = true; })
					.OnValueCommitted_Lambda([Ctx, Member](int32 V, ETextCommit::Type) { Ctx->Options.*Member = FMath::Max(1, V); Ctx->bDirty = true; })
				]
			];
			++Line;
		};
		AddCount(LOCTEXT("OptPerRow", "Per row"), LOCTEXT("OptPerRowTip", "How many people in each row."), &FFillOptions::PerRow);
		AddCount(LOCTEXT("OptRows", "Rows"), LOCTEXT("OptRowsTip", "How many rows, one behind the other."), &FFillOptions::Rows);
		AddRow(LOCTEXT("OptSeat", "Seat gap"), LOCTEXT("OptSeatTip", "Distance between people in a row, in cm. Without a marker, 0 spreads each row evenly across the mesh."), &FFillOptions::Seat, 1.f, 0.f, 1000.f, 5.f);
		AddRow(LOCTEXT("OptRow", "Row gap"), LOCTEXT("OptRowTip", "Distance between rows, in cm. Without a marker, 0 spreads the rows evenly over the mesh."), &FFillOptions::Row, 1.f, 0.f, 1000.f, 5.f);
		AddRow(LOCTEXT("OptCurve", "Curve"), LOCTEXT("OptCurveTip", "Bend the rows into arcs, in degrees. The angle is how far the rows wrap around: 0 = straight, 90 = a quarter circle curving towards the way they face (around the ring), negative values curve the other way. Everyone faces the centre of the curve."), &FFillOptions::Curve, 1.f, -270.f, 270.f, 1.f);
		AddRow(LOCTEXT("OptFill", "Fill %"), LOCTEXT("OptFillTip", "Share of the rows x people spots that get someone. Lowering it only removes people; the rest stay where they are."), &FFillOptions::Fill, 100.f, 0.f, 100.f, 1.f);
		AddRow(LOCTEXT("OptStraight", "Straightness %"), LOCTEXT("OptStraightTip", "100 = everyone exactly in line; lower scatters people forwards, backwards and sideways."), &FFillOptions::Straight, 100.f, 0.f, 100.f, 1.f);
		AddRow(LOCTEXT("OptBillboard", "Billboards from"), LOCTEXT("OptBillboardTip", "People further than this many cm from the ring are flat billboard crowd. 0 = always full models."), &FFillOptions::Billboard, 1.f, 0.f, 100000.f, 100.f,
			TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateLambda([Ctx]() { return Ctx->Options.Type == EAEWCrowdType::ByDistance; })));

		const FText Where = FText::FromString(Ctx->Markers.Num() > 0
			? FString::Printf(TEXT("%d marker(s): the crowd is centred on each and faces its X arrow. Move or rotate the marker to move the block."), Ctx->Markers.Num())
			: FString::Printf(TEXT("%d mesh(es)"), Ctx->Surfaces.Num()));

		auto Close = [WeakWindow]()
		{
			if (TSharedPtr<SWindow> W = WeakWindow.Pin())
			{
				W->RequestDestroyWindow();
			}
		};

		Window->SetContent(
			SNew(SBorder).BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder")).Padding(12.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
				[
					SNew(STextBlock).ColorAndOpacity(FSlateColor::UseSubduedForeground()).Text(Where)
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					Grid
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Text_Lambda([Ctx]()
					{
						return FText::FromString(FString::Printf(TEXT("Preview: %d people%s%s"), Ctx->PeopleCount,
							Ctx->BillboardCount > 0 ? *FString::Printf(TEXT(", %d billboards"), Ctx->BillboardCount) : TEXT(""),
							Ctx->PeopleCount > 1500 ? TEXT(" (people past the first 1500 shown as lines)") : TEXT("")));
					})
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.f, 12.f, 0.f, 0.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 6.f, 0.f)
					[
						SNew(SButton).Text(LOCTEXT("OptFillButton", "Fill")).OnClicked_Lambda([Ctx, Close]()
						{
							ComputeSpots(*Ctx);
							if (Ctx->Spots.Num() > 3000)
							{
								const FText Question = FText::FromString(FString::Printf(TEXT("This places %d people, which can be slow in game. Continue?"), Ctx->Spots.Num()));
								const FText AskTitle = LOCTEXT("FillTitle", "Fill Audience From Selected");
								if (FMessageDialog::Open(EAppMsgType::OkCancel, Question, &AskTitle) != EAppReturnType::Ok)
								{
									return FReply::Handled();
								}
							}
							SaveOptions(Ctx->Options);
							Ctx->bDone = true;
							EndPreview(*Ctx);
							SpawnCrowd(*Ctx);
							Close();
							return FReply::Handled();
						})
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton).Text(LOCTEXT("OptCancel", "Cancel")).OnClicked_Lambda([Close]()
						{
							Close();
							return FReply::Handled();
						})
					]
				]
			]);

		Window->SetOnWindowClosed(FOnWindowClosed::CreateLambda([Ctx](const TSharedRef<SWindow>&)
		{
			EndPreview(*Ctx);
		}));

		Ctx->Cleanup = FWorldDelegates::OnWorldCleanup.AddLambda([Ctx](UWorld* World, bool, bool)
		{
			if (World == Ctx->World.Get())
			{
				EndPreview(*Ctx);
				Ctx->bDone = true;
				if (TSharedPtr<SWindow> W = Ctx->Window.Pin())
				{
					W->RequestDestroyWindow();
				}
			}
		});
		Ctx->Ticker = FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Ctx](float)
		{
			if (Ctx->bDone || !Ctx->Window.IsValid())
			{
				return true;
			}
			for (FAreaMarker& M : Ctx->Markers)
			{
				if (AActor* MarkerActor = M.Actor.Get())
				{
					const FVector Location = MarkerActor->GetActorLocation();
					const float Yaw = MarkerActor->GetActorRotation().Yaw;
					if (!Location.Equals(M.Location, 0.1f) || !FMath::IsNearlyEqual(Yaw, M.Yaw, 0.01f))
					{
						M.Location = Location;
						M.Yaw = Yaw;
						Ctx->bDirty = true;
					}
				}
			}
			if (Ctx->bDirty)
			{
				Ctx->bDirty = false;
				ComputeSpots(*Ctx);
				DrawPreview(*Ctx);
			}
			return true;
		}), 0.1f);

		if (TSharedPtr<SWindow> Root = FGlobalTabmanager::Get()->GetRootWindow())
		{
			FSlateApplication::Get().AddWindowAsNativeChild(Window, Root.ToSharedRef());
		}
		else
		{
			FSlateApplication::Get().AddWindow(Window);
		}
	}
}

int32 UAEWArenaTools::FillAudienceFromSelected()
{
	const FText Title = LOCTEXT("FillTitle", "Fill Audience From Selected");
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World || GEditor->PlayWorld)
	{
		return 0;
	}
	if (TSharedPtr<SWindow> Open = ActiveFillWindow.Pin())
	{
		Open->BringToFront();
		return 0;
	}
	const UAEWArenaMakerSettings* S = GetDefault<UAEWArenaMakerSettings>();
	TSharedRef<FFillContext> Ctx = MakeShared<FFillContext>();
	Ctx->World = World;

	TArray<AActor*> SurfaceActors;
	for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
	{
		AActor* A = Cast<AActor>(*It);
		FAreaMarker Marker;
		if (MakeAreaMarker(A, Marker))
		{
			Ctx->Markers.Add(Marker);
		}
		else if (CanStandOn(A))
		{
			SurfaceActors.Add(A);
		}
	}
	if (Ctx->Markers.Num() == 0 && SurfaceActors.Num() == 0)
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("FillNone", "Select an Empty Actor where the crowd should go (Place Actors > Basic > Empty Actor), turned so its X arrow points the way the crowd should face. Or select meshes to fill them completely.\n\nThe crowd stands on whatever is under the marker (or at the marker's height where there is nothing) and the marker is removed afterwards."), &Title);
		return 0;
	}

	Ctx->Ring = FindRing(World);
	for (ULevel* Level : World->GetLevels())
	{
		CollectTemplates(Level, Ctx->Ring, Ctx->People, Ctx->Billboards);
	}
	Ctx->TemplateSource = TEXT("this arena");
	if (Ctx->People.Num() == 0 || Ctx->Billboards.Num() == 0)
	{
		if (UWorld* TemplateWorld = Cast<UWorld>(S->AudienceTemplateLevel.TryLoad()))
		{
			TArray<FCrowdTemplate> MorePeople;
			TArray<FCrowdTemplate> MoreBillboards;
			CollectTemplates(TemplateWorld->PersistentLevel, FVector::ZeroVector, MorePeople, MoreBillboards);
			const FString TemplateName = FPackageName::GetShortName(TemplateWorld->GetOutermost()->GetName());
			TArray<FString> Borrowed;
			if (Ctx->People.Num() == 0 && MorePeople.Num() > 0)
			{
				Ctx->People = MorePeople;
				Borrowed.Add(TEXT("people"));
			}
			if (Ctx->Billboards.Num() == 0 && MoreBillboards.Num() > 0)
			{
				Ctx->Billboards = MoreBillboards;
				Borrowed.Add(TEXT("billboards"));
			}
			if (Borrowed.Num() > 0)
			{
				Ctx->TemplateWorld.Reset(TemplateWorld);
				Ctx->TemplateSource += FString::Printf(TEXT(" (%s from %s)"), *FString::Join(Borrowed, TEXT(" and ")), *TemplateName);
			}
		}
	}
	if (Ctx->People.Num() == 0)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(FString::Printf(
			TEXT("No crowd people (BP_Audience) to copy: the open arena has none and Crowd Template Level (%s) could not be loaded or has none.\n\nSet Crowd Template Level in Project Settings > Arena Maker > Audience."),
			*S->AudienceTemplateLevel.ToString())), &Title);
		return 0;
	}

	auto Normalize = [](TArray<FCrowdTemplate>& List)
	{
		if (List.Num() == 0)
		{
			return;
		}
		TArray<float> Yaws;
		TArray<float> Heights;
		for (const FCrowdTemplate& T : List)
		{
			Yaws.Add(T.YawOffset);
			Heights.Add(T.ZOffset);
		}
		Yaws.Sort();
		Heights.Sort();
		const float Yaw = FMath::RoundToFloat(Yaws[Yaws.Num() / 2] / 90.f) * 90.f;
		const float Height = Heights[Heights.Num() / 2];
		for (FCrowdTemplate& T : List)
		{
			T.YawOffset = Yaw;
			T.ZOffset = Height;
		}
	};
	Normalize(Ctx->People);
	Normalize(Ctx->Billboards);


	Ctx->MinNormalZ = FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(S->AudienceMaxSlope, 0.f, 60.f)));
	Ctx->bExplicitSurfaces = SurfaceActors.Num() > 0;
	for (AActor* A : SurfaceActors)
	{
		TSharedPtr<FSurfaceData> Data = MakeShared<FSurfaceData>();
		if (Data->Build(A, Ctx->MinNormalZ, 100.f))
		{
			Ctx->Surfaces.Add(Data);
		}
	}
	if (Ctx->Markers.Num() > 0)
	{
		for (int32 i = 0; i < Ctx->Markers.Num(); ++i)
		{
			FArea Area;
			Area.Label = Ctx->Markers[i].Label;
			Area.Marker = i;
			Ctx->Areas.Add(Area);
		}
	}
	else
	{
		for (const TSharedPtr<FSurfaceData>& Data : Ctx->Surfaces)
		{
			FArea Area;
			Area.Label = Data->Label;
			Area.Under = { Data };
			const FBox& B = Data->Box;
			for (int32 i = 0; i < 8; ++i)
			{
				Area.Outline.Add(FVector((i & 1) ? B.Max.X : B.Min.X, (i & 2) ? B.Max.Y : B.Min.Y, (i & 4) ? B.Max.Z : B.Min.Z));
			}
			Ctx->Areas.Add(Area);
		}
	}

	Ctx->Options.Fill = FMath::Clamp(S->AudienceFill, 0.f, 1.f);
	Ctx->Options.Seat = FMath::Max(S->AudienceSeatSpacing, 0.f);
	Ctx->Options.Row = FMath::Max(S->AudienceRowSpacing, 0.f);
	Ctx->Options.PerRow = FMath::Clamp(S->AudiencePerRow, 1, 2000);
	Ctx->Options.Rows = FMath::Clamp(S->AudienceRows, 1, 2000);
	Ctx->Options.Curve = FMath::Clamp(S->AudienceRowCurve, -270.f, 270.f);
	if (Ctx->Markers.Num() > 0)
	{
		Ctx->Options.Seat = Ctx->Options.Seat > 0.f ? Ctx->Options.Seat : 70.f;
		Ctx->Options.Row = Ctx->Options.Row > 0.f ? Ctx->Options.Row : 90.f;
	}
	Ctx->Options.Straight = FMath::Clamp(S->AudienceStraightness, 0.f, 1.f);
	Ctx->Options.Billboard = S->AudienceBillboardDistance;
	Ctx->Options.Type = S->AudienceCrowdType;
	Ctx->bDirty = true;
	OpenFillWindow(Ctx);
	return 0;
}

int32 UAEWArenaTools::ClearGeneratedAudience()
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World || GEditor->PlayWorld)
	{
		return 0;
	}
	TSet<FName> Sources;
	int32 Selected = 0;
	for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
	{
		AActor* A = Cast<AActor>(*It);
		if (!A)
		{
			continue;
		}
		++Selected;
		if (IsGenerated(A))
		{
			for (const FName& T : A->Tags)
			{
				if (T.ToString().StartsWith(AreaPrefix))
				{
					Sources.Add(T);
				}
			}
		}
		else
		{
			Sources.Add(SourceTag(A->GetActorLabel()));
			Sources.Add(AreaTag(A->GetActorLabel()));
		}
	}
	const FScopedTransaction Transaction(LOCTEXT("ClearTitle", "Clear Generated Audience"));
	const int32 Removed = RemoveGeneratedIf(World, [&](AActor* A)
	{
		return Selected == 0 || A->Tags.ContainsByPredicate([&Sources](const FName& T) { return Sources.Contains(T); });
	});
	RemoveEmptyCrowdGroups(World);
	Toast(FString::Printf(TEXT("Removed %d generated crowd actor(s)%s."), Removed, Selected > 0 ? TEXT(" for the selection") : TEXT("")), Removed > 0);
	return Removed;
}

#undef LOCTEXT_NAMESPACE
