#include "AEWArenaTools.h"
#include "AEWArenaMakerSettings.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Engine/DataTable.h"
#include "Engine/CollisionProfile.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Event.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_CallFunction.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_Variable.h"
#include "K2Node_Message.h"
#include "K2Node_SetFieldsInStruct.h"
#include "K2Node_DynamicCast.h"
#include "UObject/EnumProperty.h"
#include "K2Node_ClassDynamicCast.h"
#include "K2Node_CallArrayFunction.h"
#include "K2Node_GetArrayItem.h"
#include "K2Node_BreakStruct.h"
#include "K2Node_MakeStruct.h"
#include "Kismet/KismetArrayLibrary.h"
#include "DataTableEditorUtils.h"
#include "Containers/Ticker.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "AEWModdingSettings.h"
#include "K2Node_GetDataTableRow.h"
#include "Kismet/KismetMathLibrary.h"
#include "Engine/LevelScriptBlueprint.h"
#include "Engine/Level.h"
#include "Engine/UserDefinedStruct.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetStringLibrary.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerStart.h"
#include "Components/CapsuleComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/BillboardComponent.h"
#include "Components/SphereComponent.h"
#include "Components/BoxComponent.h"
#include "AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "FileHelpers.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "ScopedTransaction.h"
#include "Misc/MessageDialog.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"
#include "DrawDebugHelpers.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/Selection.h"
#include "ObjectTools.h"
#include "Widgets/SWindow.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "EditorStyleSet.h"

#define LOCTEXT_NAMESPACE "AEWArenaOverrides"

DEFINE_LOG_CATEGORY_STATIC(LogAEWArenaOverrides, Log, All);

namespace
{
	const TCHAR* StartMarkerName = TEXT("BP_StartMarker");
	const TCHAR* CameraOverrideName = TEXT("BP_CameraOverride");
	const FName StartNameVar(TEXT("StartName"));
	const FName CameraTableVar(TEXT("CameraTable"));
	const FName TryApplyEvent(TEXT("TryApply"));

	const UAEWArenaMakerSettings* OvOverrideSettings()
	{
		return GetDefault<UAEWArenaMakerSettings>();
	}

	UClass* OvFindGameplayCameraClass()
	{
		for (TObjectIterator<UClass> It; It; ++It)
		{
			if (It->GetFName() == TEXT("ELGameplayCamera") && It->IsNative())
			{
				return *It;
			}
		}
		return nullptr;
	}

	UEdGraphPin* OvPin(UEdGraphNode* Node, const FName Name, EEdGraphPinDirection Dir = EGPD_MAX)
	{
		if (!Node)
		{
			return nullptr;
		}
		for (UEdGraphPin* P : Node->Pins)
		{
			if (P && P->PinName == Name && (Dir == EGPD_MAX || P->Direction == Dir))
			{
				return P;
			}
		}
		UE_LOG(LogAEWArenaOverrides, Warning, TEXT("pin %s not found on %s"), *Name.ToString(), *Node->GetName());
		return nullptr;
	}

	UEdGraphPin* OvExec(UEdGraphNode* Node)
	{
		return OvPin(Node, UEdGraphSchema_K2::PN_Execute, EGPD_Input);
	}

	UEdGraphPin* OvThen(UEdGraphNode* Node)
	{
		return OvPin(Node, UEdGraphSchema_K2::PN_Then, EGPD_Output);
	}

	UEdGraphPin* OvOut(UEdGraphNode* Node)
	{
		return OvPin(Node, UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output);
	}

	UEdGraphPin* OvTarget(UEdGraphNode* Node)
	{
		return OvPin(Node, UEdGraphSchema_K2::PN_Self, EGPD_Input);
	}

	bool OvLink(UEdGraphPin* A, UEdGraphPin* B)
	{
		if (!A || !B)
		{
			return false;
		}
		const bool bOk = GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(A, B);
		if (!bOk)
		{
			UE_LOG(LogAEWArenaOverrides, Warning, TEXT("could not connect %s.%s to %s.%s"),
				*A->GetOwningNode()->GetName(), *A->PinName.ToString(), *B->GetOwningNode()->GetName(), *B->PinName.ToString());
		}
		return bOk;
	}

	void OvSetValue(UEdGraphPin* P, const FString& Value)
	{
		if (P)
		{
			GetDefault<UEdGraphSchema_K2>()->TrySetDefaultValue(*P, Value);
		}
	}

	void OvSetObject(UEdGraphPin* P, UObject* Value)
	{
		if (P)
		{
			GetDefault<UEdGraphSchema_K2>()->TrySetDefaultObject(*P, Value);
			P->GetOwningNode()->ReconstructNode();
		}
	}

	template <typename T>
	T* OvPlace(UEdGraph* Graph, T* Node, int32 X, int32 Y)
	{
		Graph->AddNode(Node, false, false);
		Node->CreateNewGuid();
		Node->PostPlacedNewNode();
		Node->AllocateDefaultPins();
		Node->NodePosX = X;
		Node->NodePosY = Y;
		return Node;
	}

	UK2Node_CallFunction* OvCall(UEdGraph* Graph, UClass* Class, const FName Function, int32 X, int32 Y)
	{
		UFunction* F = Class->FindFunctionByName(Function);
		if (!F)
		{
			UE_LOG(LogAEWArenaOverrides, Error, TEXT("function %s::%s not found"), *Class->GetName(), *Function.ToString());
			return nullptr;
		}
		UK2Node_CallFunction* Node = NewObject<UK2Node_CallFunction>(Graph);
		Node->SetFromFunction(F);
		return OvPlace(Graph, Node, X, Y);
	}

	UK2Node_Event* OvBeginPlayNode(UEdGraph* Graph, int32 X, int32 Y)
	{
		UK2Node_Event* Node = NewObject<UK2Node_Event>(Graph);
		Node->EventReference.SetExternalMember(TEXT("ReceiveBeginPlay"), AActor::StaticClass());
		Node->bOverrideFunction = true;
		return OvPlace(Graph, Node, X, Y);
	}

	UK2Node_IfThenElse* OvBranch(UEdGraph* Graph, int32 X, int32 Y)
	{
		return OvPlace(Graph, NewObject<UK2Node_IfThenElse>(Graph), X, Y);
	}

	UK2Node_VariableGet* OvGetSelfVar(UEdGraph* Graph, const FName Name, int32 X, int32 Y)
	{
		UK2Node_VariableGet* Node = NewObject<UK2Node_VariableGet>(Graph);
		Node->VariableReference.SetSelfMember(Name);
		return OvPlace(Graph, Node, X, Y);
	}

	AActor* OvFindActorOfClassName(UWorld* World, const TCHAR* ClassName)
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->GetClass()->GetName() == ClassName)
			{
				return *It;
			}
		}
		return nullptr;
	}

	UEdGraphPin* OvLoopBody(UEdGraphNode* Loop)
	{
		for (UEdGraphPin* P : Loop->Pins)
		{
			if (P && P->Direction == EGPD_Output && P->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec && P->PinName != TEXT("Completed"))
			{
				return P;
			}
		}
		UE_LOG(LogAEWArenaOverrides, Warning, TEXT("loop body pin not found on %s"), *Loop->GetName());
		return nullptr;
	}

	UEdGraph* OvForEachMacro()
	{
		UBlueprint* Macros = LoadObject<UBlueprint>(nullptr, TEXT("/Engine/EditorBlueprintResources/StandardMacros.StandardMacros"));
		if (Macros)
		{
			for (UEdGraph* Graph : Macros->MacroGraphs)
			{
				if (Graph && Graph->GetFName() == TEXT("ForEachLoop"))
				{
					return Graph;
				}
			}
		}
		return nullptr;
	}

	void OvAddVariable(UBlueprint* BP, const FName Name, const FName Category, UObject* SubObject, const FString& Tooltip)
	{
		FEdGraphPinType Type;
		Type.PinCategory = Category;
		Type.PinSubCategoryObject = SubObject;
		FBlueprintEditorUtils::AddMemberVariable(BP, Name, Type);
		FBlueprintEditorUtils::SetBlueprintOnlyEditableFlag(BP, Name, false);
		FBlueprintEditorUtils::SetBlueprintVariableMetaData(BP, Name, nullptr, TEXT("tooltip"), Tooltip);
		FBlueprintEditorUtils::SetBlueprintVariableCategory(BP, Name, nullptr, FText::FromString(TEXT("Arena Override")));
	}

	UBlueprint* OvNewActorBlueprint(const FString& Folder, const FString& Name, bool bRebuild, bool& bOutNew)
	{
		bOutNew = false;
		const FString PackageName = Folder / Name;
		if (UBlueprint* Existing = LoadObject<UBlueprint>(nullptr, *(PackageName + TEXT(".") + Name), nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			if (!bRebuild)
			{
				return Existing;
			}
			UEdGraph* Graph = FBlueprintEditorUtils::FindEventGraph(Existing);
			if (Graph)
			{
				TArray<UEdGraphNode*> Nodes = Graph->Nodes;
				for (UEdGraphNode* Node : Nodes)
				{
					FBlueprintEditorUtils::RemoveNode(Existing, Node, true);
				}
			}
			return Existing;
		}
		UPackage* Package = CreatePackage(*PackageName);
		UBlueprint* BP = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), Package, FName(*Name), BPTYPE_Normal,
			UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), FName(TEXT("AEWArenaTools")));
		if (!BP)
		{
			return nullptr;
		}
		FAssetRegistryModule::AssetCreated(BP);
		UEdGraph* Graph = FBlueprintEditorUtils::FindEventGraph(BP);
		if (Graph)
		{
			TArray<UEdGraphNode*> Nodes = Graph->Nodes;
			for (UEdGraphNode* Node : Nodes)
			{
				FBlueprintEditorUtils::RemoveNode(BP, Node, true);
			}
		}
		bOutNew = true;
		return BP;
	}

	bool OvFinishBlueprint(UBlueprint* BP)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
		FKismetEditorUtilities::CompileBlueprint(BP);
		UEditorLoadingAndSavingUtils::SavePackages({ BP->GetOutermost() }, false);
		if (BP->Status == BS_Error)
		{
			UE_LOG(LogAEWArenaOverrides, Error, TEXT("%s has compile errors - open it to see them"), *BP->GetName());
			return false;
		}
		return true;
	}

	USCS_Node* OvAddComponent(UBlueprint* BP, UClass* Class, const FName Name, USCS_Node* Parent)
	{
		USimpleConstructionScript* SCS = BP->SimpleConstructionScript;
		USCS_Node* Node = SCS->CreateNode(Class, Name);
		if (Parent)
		{
			Parent->AddChildNode(Node);
		}
		else
		{
			SCS->AddNode(Node);
		}
		return Node;
	}
}

namespace
{
	bool OvHasCustomEvent(UBlueprint* BP, const FName Name)
	{
		UEdGraph* Graph = BP ? FBlueprintEditorUtils::FindEventGraph(BP) : nullptr;
		if (!Graph)
		{
			return false;
		}
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			UK2Node_CustomEvent* Event = Cast<UK2Node_CustomEvent>(Node);
			if (Event && Event->CustomFunctionName == Name)
			{
				return true;
			}
		}
		return false;
	}

	bool OvBuildMoveStarts(UEdGraph* G, UEdGraphPin* From, int32 X, int32 Y)
	{
		UK2Node_CallFunction* GetAll = OvCall(G, UGameplayStatics::StaticClass(), GET_FUNCTION_NAME_CHECKED(UGameplayStatics, GetAllActorsOfClass), X, Y);
		OvSetObject(OvPin(GetAll, TEXT("ActorClass"), EGPD_Input), APlayerStart::StaticClass());
		UK2Node_MacroInstance* Loop = NewObject<UK2Node_MacroInstance>(G);
		Loop->SetMacroGraph(OvForEachMacro());
		OvPlace(G, Loop, X + 350, Y);
		UK2Node_CallFunction* ObjName = OvCall(G, UKismetSystemLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, GetObjectName), X + 350, Y + 250);
		UK2Node_VariableGet* Wanted = OvGetSelfVar(G, StartNameVar, X + 350, Y + 400);
		UK2Node_CallFunction* Equal = OvCall(G, UKismetStringLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetStringLibrary, EqualEqual_StrStr), X + 650, Y + 300);
		UK2Node_IfThenElse* If = OvBranch(G, X + 900, Y);
		UK2Node_CallFunction* Root = OvCall(G, AActor::StaticClass(), GET_FUNCTION_NAME_CHECKED(AActor, K2_GetRootComponent), X + 900, Y + 250);
		UK2Node_CallFunction* Mobility = OvCall(G, USceneComponent::StaticClass(), GET_FUNCTION_NAME_CHECKED(USceneComponent, SetMobility), X + 1150, Y);
		OvSetValue(OvPin(Mobility, TEXT("NewMobility"), EGPD_Input), TEXT("Movable"));
		UK2Node_CallFunction* MyLoc = OvCall(G, AActor::StaticClass(), GET_FUNCTION_NAME_CHECKED(AActor, K2_GetActorLocation), X + 1150, Y + 250);
		UK2Node_CallFunction* MyRot = OvCall(G, AActor::StaticClass(), GET_FUNCTION_NAME_CHECKED(AActor, K2_GetActorRotation), X + 1150, Y + 350);
		UK2Node_CallFunction* Move = OvCall(G, AActor::StaticClass(), GET_FUNCTION_NAME_CHECKED(AActor, K2_SetActorLocationAndRotation), X + 1450, Y);
		OvSetValue(OvPin(Move, TEXT("bTeleport"), EGPD_Input), TEXT("true"));
		if (!GetAll || !ObjName || !Equal || !Root || !Mobility || !MyLoc || !MyRot || !Move || !Loop->GetMacroGraph())
		{
			return false;
		}

		OvLink(From, OvExec(GetAll));
		OvLink(OvThen(GetAll), OvPin(Loop, TEXT("Exec"), EGPD_Input));
		OvLink(OvPin(GetAll, TEXT("OutActors"), EGPD_Output), OvPin(Loop, TEXT("Array"), EGPD_Input));
		OvLink(OvLoopBody(Loop), OvExec(If));
		OvLink(OvPin(Loop, TEXT("Array Element"), EGPD_Output), OvPin(ObjName, TEXT("Object"), EGPD_Input));
		OvLink(OvOut(ObjName), OvPin(Equal, TEXT("A"), EGPD_Input));
		OvLink(OvPin(Wanted, StartNameVar, EGPD_Output), OvPin(Equal, TEXT("B"), EGPD_Input));
		OvLink(OvOut(Equal), If->GetConditionPin());
		OvLink(OvPin(Loop, TEXT("Array Element"), EGPD_Output), OvTarget(Root));
		OvLink(If->GetThenPin(), OvExec(Mobility));
		OvLink(OvOut(Root), OvTarget(Mobility));
		OvLink(OvThen(Mobility), OvExec(Move));
		OvLink(OvPin(Loop, TEXT("Array Element"), EGPD_Output), OvTarget(Move));
		OvLink(OvOut(MyLoc), OvPin(Move, TEXT("NewLocation"), EGPD_Input));
		OvLink(OvOut(MyRot), OvPin(Move, TEXT("NewRotation"), EGPD_Input));
		return true;
	}
}

UBlueprint* UAEWArenaTools::EnsureStartMarkerBlueprint(const FString& ArenaFolder, bool bRebuild)
{
	if (!bRebuild)
	{
		UBlueprint* Existing = LoadObject<UBlueprint>(nullptr, *((ArenaFolder / StartMarkerName) + TEXT(".") + StartMarkerName), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (Existing && !OvHasCustomEvent(Existing, TryApplyEvent))
		{
			UE_LOG(LogAEWArenaOverrides, Log, TEXT("updating %s to re-apply its start every 0.1 s"), *Existing->GetPathName());
			bRebuild = true;
		}
	}
	bool bNew = false;
	UBlueprint* BP = OvNewActorBlueprint(ArenaFolder, StartMarkerName, bRebuild, bNew);
	if (!BP || (!bNew && !bRebuild))
	{
		return BP;
	}

	if (bNew)
	{
		USCS_Node* CapsuleNode = OvAddComponent(BP, UCapsuleComponent::StaticClass(), TEXT("Capsule"), nullptr);
		if (UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(CapsuleNode->ComponentTemplate))
		{
			Capsule->InitCapsuleSize(34.f, 88.f);
			Capsule->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
			Capsule->SetHiddenInGame(true);
			Capsule->ShapeColor = FColor(255, 140, 0);
		}
		USCS_Node* ArrowNode = OvAddComponent(BP, UArrowComponent::StaticClass(), TEXT("Facing"), CapsuleNode);
		if (UArrowComponent* Arrow = Cast<UArrowComponent>(ArrowNode->ComponentTemplate))
		{
			Arrow->ArrowColor = FColor(255, 140, 0);
			Arrow->ArrowSize = 1.5f;
			Arrow->SetHiddenInGame(true);
		}
		OvAddVariable(BP, StartNameVar, UEdGraphSchema_K2::PC_String, nullptr,
			TEXT("ArenaMatch start to move here, e.g. PlayerStart_1on1_00."));
	}

	UEdGraph* G = FBlueprintEditorUtils::FindEventGraph(BP);
	UK2Node_Event* Begin = OvBeginPlayNode(G, 0, 0);
	UK2Node_CallFunction* StartTimer = OvCall(G, UKismetSystemLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, K2_SetTimer), 250, 0);
	OvSetValue(OvPin(StartTimer, TEXT("FunctionName"), EGPD_Input), TryApplyEvent.ToString());
	OvSetValue(OvPin(StartTimer, TEXT("Time"), EGPD_Input), TEXT("0.1"));
	OvSetValue(OvPin(StartTimer, TEXT("bLooping"), EGPD_Input), TEXT("true"));
	if (!StartTimer)
	{
		return nullptr;
	}
	OvLink(OvThen(Begin), OvExec(StartTimer));

	UK2Node_CustomEvent* Try = NewObject<UK2Node_CustomEvent>(G);
	Try->CustomFunctionName = TryApplyEvent;
	OvPlace(G, Try, 0, 700);

	if (!OvBuildMoveStarts(G, OvThen(StartTimer), 600, 0) || !OvBuildMoveStarts(G, OvThen(Try), 250, 700))
	{
		return nullptr;
	}

	OvFinishBlueprint(BP);
	return BP;
}

UBlueprint* UAEWArenaTools::EnsureCameraOverrideBlueprint(const FString& ArenaFolder, bool bRebuild, UDataTable* Table)
{
	UClass* CameraClass = OvFindGameplayCameraClass();
	if (!CameraClass)
	{
		UE_LOG(LogAEWArenaOverrides, Error, TEXT("ELGameplayCamera not found. Is the game module loaded?"));
		return nullptr;
	}
	if (!Table)
	{
		UE_LOG(LogAEWArenaOverrides, Error, TEXT("no camera table for BP_CameraOverride"));
		return nullptr;
	}
	bool bNew = false;
	UBlueprint* BP = OvNewActorBlueprint(ArenaFolder, CameraOverrideName, bRebuild, bNew);
	if (!BP || (!bNew && !bRebuild))
	{
		return BP;
	}

	if (bNew)
	{
		OvAddComponent(BP, UBillboardComponent::StaticClass(), TEXT("Icon"), nullptr);
		OvAddVariable(BP, CameraTableVar, UEdGraphSchema_K2::PC_Object, UDataTable::StaticClass(),
			TEXT("Camera table this arena uses instead of DT_GameplayCameraParam."));
	}

	UEdGraph* G = FBlueprintEditorUtils::FindEventGraph(BP);
	UK2Node_Event* Tick = NewObject<UK2Node_Event>(G);
	Tick->EventReference.SetExternalMember(TEXT("ReceiveTick"), AActor::StaticClass());
	Tick->bOverrideFunction = true;
	OvPlace(G, Tick, 0, 0);
	UK2Node_CallFunction* Find = OvCall(G, UGameplayStatics::StaticClass(), GET_FUNCTION_NAME_CHECKED(UGameplayStatics, GetActorOfClass), 250, 0);
	OvSetObject(OvPin(Find, TEXT("ActorClass"), EGPD_Input), CameraClass);
	UK2Node_CallFunction* Valid = OvCall(G, UKismetSystemLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, IsValid), 550, 200);
	UK2Node_IfThenElse* If = OvBranch(G, 750, 0);
	UK2Node_VariableGet* Row = NewObject<UK2Node_VariableGet>(G);
	Row->VariableReference.SetExternalMember(TEXT("RowName"), CameraClass);
	OvPlace(G, Row, 750, 250);
	UK2Node_GetDataTableRow* GetRow = NewObject<UK2Node_GetDataTableRow>(G);
	OvPlace(G, GetRow, 1000, 0);
	OvSetObject(GetRow->GetDataTablePin(), Table);
	UK2Node_VariableSet* SetParam = NewObject<UK2Node_VariableSet>(G);
	SetParam->VariableReference.SetExternalMember(TEXT("GameplayCameraParam"), CameraClass);
	OvPlace(G, SetParam, 1400, 0);
	if (!Find || !Valid)
	{
		return nullptr;
	}

	OvLink(OvThen(Tick), OvExec(Find));
	OvLink(OvThen(Find), OvExec(If));
	OvLink(OvOut(Find), OvPin(Valid, TEXT("Object"), EGPD_Input));
	OvLink(OvOut(Valid), If->GetConditionPin());
	OvLink(If->GetThenPin(), OvExec(GetRow));
	OvLink(OvOut(Find), OvTarget(Row));
	OvLink(OvPin(Row, TEXT("RowName"), EGPD_Output), GetRow->GetRowNamePin());
	OvLink(GetRow->GetThenPin(), OvExec(SetParam));
	OvLink(OvOut(Find), OvTarget(SetParam));
	OvLink(GetRow->GetResultPin(), OvPin(SetParam, TEXT("GameplayCameraParam"), EGPD_Input));

	OvFinishBlueprint(BP);
	return BP;
}

namespace
{
	UWorld* OvEditorWorld()
	{
		return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	}

	FString OvArenaFolderOf(UWorld* World)
	{
		return FPackageName::GetLongPackagePath(World->GetOutermost()->GetName());
	}

	void OvReport(const FString& Message)
	{
		UE_LOG(LogAEWArenaOverrides, Log, TEXT("%s"), *Message.Replace(TEXT("\n"), TEXT(" ")));
		const FText Title = LOCTEXT("Title", "Arena Overrides");
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Message), &Title);
	}

	UDataTable* OvEnsureArenaCameraTable(const FString& Folder, const FString& ArenaName, bool& bOutNew)
	{
		bOutNew = false;
		IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		TArray<FAssetData> Assets;
		AR.GetAssetsByPath(FName(*Folder), Assets, true);
		for (const FAssetData& A : Assets)
		{
			if (A.AssetClass == UDataTable::StaticClass()->GetFName() && A.AssetName.ToString().StartsWith(TEXT("DT_GameplayCameraParam")))
			{
				return Cast<UDataTable>(A.GetAsset());
			}
		}
		UDataTable* Source = Cast<UDataTable>(OvOverrideSettings()->GameCameraTable.TryLoad());
		if (!Source)
		{
			return nullptr;
		}
		IAssetTools& Tools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		UDataTable* Copy = Cast<UDataTable>(Tools.DuplicateAsset(TEXT("DT_GameplayCameraParam_") + ArenaName, Folder, Source));
		if (!Copy)
		{
			return nullptr;
		}
		UEditorLoadingAndSavingUtils::SavePackages({ Copy->GetOutermost() }, false);
		bOutNew = true;
		return Copy;
	}
}

int32 UAEWArenaTools::AddStartMarkers()
{
	UWorld* World = OvEditorWorld();
	if (!World)
	{
		return 0;
	}
	UBlueprint* BP = EnsureStartMarkerBlueprint(OvArenaFolderOf(World), true);
	UClass* MarkerClass = BP ? BP->GeneratedClass.Get() : nullptr;
	FStrProperty* NameProp = MarkerClass ? FindFProperty<FStrProperty>(MarkerClass, StartNameVar) : nullptr;
	if (!NameProp)
	{
		OvReport(TEXT("Could not create BP_StartMarker - see the Output Log."));
		return 0;
	}

	TSet<FString> Existing;
	TArray<APlayerStart*> Starts;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->GetClass() == MarkerClass)
		{
			Existing.Add(NameProp->GetPropertyValue_InContainer(*It));
		}
		else if (APlayerStart* Start = Cast<APlayerStart>(*It))
		{
			if (FPackageName::GetShortName(Start->GetLevel()->GetOutermost()->GetName()) == TEXT("ArenaMatch"))
			{
				Starts.Add(Start);
			}
		}
	}
	if (Starts.Num() == 0)
	{
		OvReport(TEXT("No starts found. Load the arena's ArenaMatch sublevel."));
		return 0;
	}
	Starts.Sort([](const APlayerStart& A, const APlayerStart& B) { return A.GetName() < B.GetName(); });

	const FScopedTransaction Transaction(LOCTEXT("AddStartMarkers", "Add Start Markers"));
	World->PersistentLevel->Modify();
	int32 Added = 0;
	for (APlayerStart* Start : Starts)
	{
		const FString Name = Start->GetName();
		if (Existing.Contains(Name))
		{
			continue;
		}
		FActorSpawnParameters Params;
		Params.OverrideLevel = World->PersistentLevel;
		AActor* Marker = World->SpawnActor<AActor>(MarkerClass, Start->GetActorTransform(), Params);
		if (!Marker)
		{
			continue;
		}
		NameProp->SetPropertyValue_InContainer(Marker, Name);
		Marker->SetActorLabel(TEXT("Start_") + Name);
		Marker->SetFolderPath(TEXT("ArenaOverrides/Starts"));
		++Added;
	}
	World->PersistentLevel->MarkPackageDirty();
	OvReport(FString::Printf(TEXT("Added %d start marker(s) to %s (%d already there).\n\nMove the ones to change, delete the rest, then save."),
		Added, *World->GetMapName(), Existing.Num()));
	return Added;
}

bool UAEWArenaTools::AddCameraOverride()
{
	UWorld* World = OvEditorWorld();
	if (!World)
	{
		return false;
	}
	const FString Folder = OvArenaFolderOf(World);
	bool bNewTable = false;
	UDataTable* Table = nullptr;
	if (AActor* Placed = OvFindActorOfClassName(World, TEXT("BP_CameraOverride_C")))
	{
		if (FObjectProperty* P = FindFProperty<FObjectProperty>(Placed->GetClass(), CameraTableVar))
		{
			Table = Cast<UDataTable>(P->GetObjectPropertyValue_InContainer(Placed));
		}
	}
	if (!Table)
	{
		Table = OvEnsureArenaCameraTable(Folder, FPackageName::GetShortName(World->GetOutermost()->GetName()), bNewTable);
	}
	UBlueprint* BP = EnsureCameraOverrideBlueprint(Folder, true, Table);
	UClass* OverrideClass = BP ? BP->GeneratedClass.Get() : nullptr;
	FObjectProperty* TableProp = OverrideClass ? FindFProperty<FObjectProperty>(OverrideClass, CameraTableVar) : nullptr;
	if (!TableProp)
	{
		OvReport(TEXT("Could not create BP_CameraOverride - see the Output Log."));
		return false;
	}

	AActor* Override = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->GetClass() == OverrideClass)
		{
			Override = *It;
			break;
		}
	}
	const FScopedTransaction Transaction(LOCTEXT("AddCameraOverride", "Add Camera Override"));
	const bool bNewActor = Override == nullptr;
	if (!Override)
	{
		FActorSpawnParameters Params;
		Params.OverrideLevel = World->PersistentLevel;
		Override = World->SpawnActor<AActor>(OverrideClass, FTransform(FVector(0.f, 0.f, 300.f)), Params);
		if (!Override)
		{
			return false;
		}
		Override->SetActorLabel(TEXT("ArenaCameraOverride"));
		Override->SetFolderPath(TEXT("ArenaOverrides"));
	}
	Override->Modify();
	if (Table && !TableProp->GetObjectPropertyValue_InContainer(Override))
	{
		TableProp->SetObjectPropertyValue_InContainer(Override, Table);
	}
	World->PersistentLevel->MarkPackageDirty();

	FString Message = bNewActor ? TEXT("Added an ArenaCameraOverride to the map.") : TEXT("The map already has an ArenaCameraOverride.");
	if (Table)
	{
		Message += FString::Printf(TEXT("\n\nCamera table: %s%s"), *Table->GetName(),
			bNewTable ? TEXT(" (new copy)") : TEXT(""));
		Message += TEXT("\nEdit its rows to tune the camera, then save the map.");
	}
	else
	{
		Message += TEXT("\n\nNo camera table found. Set Camera Table on the actor.");
	}
	OvReport(Message);
	return true;
}

namespace
{
	const TCHAR* GameInstanceInterfacePath = TEXT("/Game/GameData/Assets/GameInstance/BPI_ELGameInstance.BPI_ELGameInstance_C");
	const TCHAR* MatchRuleStructPath = TEXT("/Game/GameData/Assets/Struct/MatchRule/FMatchRule.FMatchRule");
	const FName OriginalRuleVar(TEXT("OriginalMatchRule"));

	UK2Node_Event* OvFindEvent(UEdGraph* Graph, const FName EventName)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			UK2Node_Event* Event = Cast<UK2Node_Event>(Node);
			if (Event && Event->EventReference.GetMemberName() == EventName)
			{
				return Event;
			}
		}
		return nullptr;
	}

	UK2Node_Event* OvEventNode(UEdGraph* Graph, const FName EventName, int32 X, int32 Y)
	{
		if (UK2Node_Event* Existing = OvFindEvent(Graph, EventName))
		{
			Existing->SetEnabledState(ENodeEnabledState::Enabled);
			return Existing;
		}
		UK2Node_Event* Node = NewObject<UK2Node_Event>(Graph);
		Node->EventReference.SetExternalMember(EventName, AActor::StaticClass());
		Node->bOverrideFunction = true;
		return OvPlace(Graph, Node, X, Y);
	}

	UK2Node_Message* OvMessage(UEdGraph* Graph, UFunction* Function, int32 X, int32 Y)
	{
		UK2Node_Message* Node = NewObject<UK2Node_Message>(Graph);
		Node->SetFromFunction(Function);
		return OvPlace(Graph, Node, X, Y);
	}

	UEdGraphPin* OvPinOfType(UEdGraphNode* Node, EEdGraphPinDirection Dir, UObject* StructType)
	{
		for (UEdGraphPin* P : Node->Pins)
		{
			if (P && P->Direction == Dir && P->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct && P->PinType.PinSubCategoryObject == StructType)
			{
				return P;
			}
		}
		UE_LOG(LogAEWArenaOverrides, Warning, TEXT("no %s match rule pin on %s"), Dir == EGPD_Input ? TEXT("input") : TEXT("output"), *Node->GetName());
		return nullptr;
	}

	void OvInsertAfter(UEdGraphPin* From, UEdGraphPin* ChainIn, UEdGraphPin* ChainOut)
	{
		if (!From || !ChainIn || !ChainOut)
		{
			return;
		}
		TArray<UEdGraphPin*> Old = From->LinkedTo;
		From->BreakAllPinLinks();
		OvLink(From, ChainIn);
		for (UEdGraphPin* Next : Old)
		{
			OvLink(ChainOut, Next);
		}
	}

	int32 OvLowestY(UEdGraph* Graph)
	{
		int32 Y = 0;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			Y = FMath::Max(Y, Node->NodePosY);
		}
		return Y + 500;
	}
}

namespace
{
	const FName CameraPreviewTag(TEXT("AEWCameraPreview"));

	float OvRowFloat(const UScriptStruct* RS, const uint8* Row, const TCHAR* Name, float Default)
	{
		FFloatProperty* P = FindFProperty<FFloatProperty>(RS, Name);
		return P ? P->GetPropertyValue_InContainer(Row) : Default;
	}

	FVector OvRowVector(const UScriptStruct* RS, const uint8* Row, const TCHAR* Name)
	{
		FStructProperty* P = FindFProperty<FStructProperty>(RS, Name);
		if (P && P->Struct == TBaseStructure<FVector>::Get())
		{
			return *P->ContainerPtrToValuePtr<FVector>(Row);
		}
		return FVector::ZeroVector;
	}

	struct FOvCameraParams
	{
		FVector TargetOffset = FVector(0.f, 0.f, 5.f);
		float OffZMin = -30.f;
		float OffZMax = 100.f;
		float RMinRing = 140.f;
		float RMaxRing = 403.f;
		float RMinArena = 200.f;
		float RMaxArena = 1100.f;
		float PitchMin = -12.f;
		float PitchMax = -10.f;
		float RateU = 0.8f;
		float RateD = 0.8f;
		float RateX = 0.8f;
		float RateY = 0.8f;
	};

	bool OvReadCameraRow(UDataTable* Table, const FName Row, FOvCameraParams& Out)
	{
		const UScriptStruct* RS = Table ? Table->GetRowStruct() : nullptr;
		const uint8* const* Found = Table ? Table->GetRowMap().Find(Row) : nullptr;
		if (!RS || !Found || !*Found)
		{
			return false;
		}
		const uint8* R = *Found;
		Out.TargetOffset = OvRowVector(RS, R, TEXT("TargetOffset"));
		Out.OffZMin = OvRowFloat(RS, R, TEXT("CameraOffsetZ_Min"), Out.OffZMin);
		Out.OffZMax = OvRowFloat(RS, R, TEXT("CameraOffsetZ_Max"), Out.OffZMax);
		Out.RMinRing = OvRowFloat(RS, R, TEXT("RadiusMin_Ring"), Out.RMinRing);
		Out.RMaxRing = OvRowFloat(RS, R, TEXT("RadiusMax_Ring"), Out.RMaxRing);
		Out.RMinArena = OvRowFloat(RS, R, TEXT("RadiusMin_Arena"), Out.RMinArena);
		Out.RMaxArena = OvRowFloat(RS, R, TEXT("RadiusMax_Arena"), Out.RMaxArena);
		Out.PitchMin = OvRowFloat(RS, R, TEXT("Pitch_Min"), Out.PitchMin);
		Out.PitchMax = OvRowFloat(RS, R, TEXT("Pitch_Max"), Out.PitchMax);
		Out.RateU = OvRowFloat(RS, R, TEXT("LocationRate_U"), Out.RateU);
		Out.RateD = OvRowFloat(RS, R, TEXT("LocationRate_D"), Out.RateD);
		Out.RateX = OvRowFloat(RS, R, TEXT("LocationRate_X"), Out.RateX);
		Out.RateY = OvRowFloat(RS, R, TEXT("LocationRate_Y"), Out.RateY);
		return true;
	}

	FVector OvFitPoints(const TArray<FVector>& Points, const FRotator& Rot, float FovDeg, float Aspect, float Pad)
	{
		const float TanH = FMath::Tan(FMath::DegreesToRadians(FovDeg * 0.5f));
		const float H = FMath::Atan(TanH);
		const float V = FMath::Atan(TanH / FMath::Max(Aspect, 0.01f));
		const float SH = FMath::Sin(H), CH = FMath::Cos(H), SV = FMath::Sin(V), CV = FMath::Cos(V);
		float D1 = MAX_flt, D2 = MAX_flt, D3 = MAX_flt, D4 = MAX_flt;
		for (const FVector& P : Points)
		{
			const FVector L = Rot.UnrotateVector(P);
			D1 = FMath::Min(D1, SH * L.X + CH * L.Y - Pad);
			D2 = FMath::Min(D2, SH * L.X - CH * L.Y - Pad);
			D3 = FMath::Min(D3, SV * L.X + CV * L.Z - Pad);
			D4 = FMath::Min(D4, SV * L.X - CV * L.Z - Pad);
		}
		const float XH = (D1 + D2) / (2.f * SH);
		const float XV = (D3 + D4) / (2.f * SV);
		const FVector Local(FMath::Min(XH, XV), (D1 - D2) / (2.f * CH), (D3 - D4) / (2.f * CV));
		return Rot.RotateVector(Local);
	}

	FVector OvClampFromTarget(const FVector& Target, const FVector& Point, float RMin, float RMax)
	{
		const FVector Delta = Point - Target;
		const float Dist = Delta.Size();
		const FVector Dir = Delta.SizeSquared() < 1e-8f ? FVector::ZeroVector : Delta / Dist;
		const float Clamped = Dist < RMin ? RMin : FMath::Min(Dist, RMax);
		return Target + Dir * Clamped;
	}

	FVector OvStartLocation(UWorld* World, const FString& StartName, bool& bFound)
	{
		bFound = false;
		FVector Result = FVector::ZeroVector;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->GetClass()->GetName() == TEXT("BP_StartMarker_C"))
			{
				FStrProperty* NameProp = FindFProperty<FStrProperty>(It->GetClass(), StartNameVar);
				if (NameProp && NameProp->GetPropertyValue_InContainer(*It) == StartName)
				{
					bFound = true;
					return It->GetActorLocation();
				}
			}
		}
		for (TActorIterator<APlayerStart> It(World); It; ++It)
		{
			if (It->GetName() == StartName)
			{
				bFound = true;
				Result = It->GetActorLocation();
			}
		}
		return Result;
	}

	void OvUnwatchCameraPreview();

	void OvClearCameraPreview(UWorld* World)
	{
		OvUnwatchCameraPreview();
		FlushPersistentDebugLines(World);
		TArray<AActor*> Old;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->ActorHasTag(CameraPreviewTag))
			{
				Old.Add(*It);
			}
		}
		for (AActor* A : Old)
		{
			World->EditorDestroyActor(A, false);
		}
	}
}

void UAEWArenaTools::ClearCameraPreview()
{
	if (UWorld* World = OvEditorWorld())
	{
		OvClearCameraPreview(World);
		GEditor->RedrawLevelEditingViewports();
	}
}

namespace
{
	UDataTable* OvCameraPreviewTable(UWorld* World, bool& bOutArenaTable)
	{
		UDataTable* Table = nullptr;
		if (AActor* Override = OvFindActorOfClassName(World, TEXT("BP_CameraOverride_C")))
		{
			if (FObjectProperty* P = FindFProperty<FObjectProperty>(Override->GetClass(), CameraTableVar))
			{
				Table = Cast<UDataTable>(P->GetObjectPropertyValue_InContainer(Override));
			}
		}
		bOutArenaTable = Table != nullptr;
		if (!Table)
		{
			Table = Cast<UDataTable>(OvOverrideSettings()->GameCameraTable.TryLoad());
		}
		return Table;
	}

	ACameraActor* OvFindCameraPreview(UWorld* World)
	{
		for (TActorIterator<ACameraActor> It(World); It; ++It)
		{
			if (It->ActorHasTag(CameraPreviewTag))
			{
				return *It;
			}
		}
		return nullptr;
	}

	bool OvBuildCameraPreview(bool bInitial);

	void OvQueueCameraPreviewRefresh();

	class FOvCameraTableListener : public FDataTableEditorUtils::INotifyOnDataTableChanged
	{
	public:
		TWeakObjectPtr<UDataTable> Table;

		virtual void PreChange(const UDataTable* Changed, FDataTableEditorUtils::EDataTableChangeInfo Info) override {}

		virtual void PostChange(const UDataTable* Changed, FDataTableEditorUtils::EDataTableChangeInfo Info) override
		{
			const UDataTable* Watched = Table.Get();
			if (Changed && Watched && (Changed == Watched || Changed->RowStruct == Watched->RowStruct))
			{
				OvQueueCameraPreviewRefresh();
			}
		}
	};

	struct FOvCameraPreviewWatch
	{
		FOvCameraTableListener* Listener = nullptr;
		TWeakObjectPtr<UDataTable> Table;
		FDelegateHandle TableHandle;
		FDelegateHandle MovedHandle;
		FDelegateHandle AddedHandle;
		FDelegateHandle DeletedHandle;
		FDelegateHandle PropertyHandle;
		FDelegateHandle TickHandle;
		TArray<TWeakObjectPtr<AActor>> StandIns;
		TWeakPtr<SNotificationItem> Toast;
	};

	FOvCameraPreviewWatch& OvCamWatch()
	{
		static FOvCameraPreviewWatch Watch;
		return Watch;
	}

	void OvUnwatchCameraPreview()
	{
		FOvCameraPreviewWatch& W = OvCamWatch();
		if (UDataTable* T = W.Table.Get())
		{
			T->OnDataTableChanged().Remove(W.TableHandle);
		}
		if (GEngine && W.MovedHandle.IsValid())
		{
			GEngine->OnActorMoved().Remove(W.MovedHandle);
		}
		if (GEngine && W.AddedHandle.IsValid())
		{
			GEngine->OnLevelActorAdded().Remove(W.AddedHandle);
		}
		if (GEngine && W.DeletedHandle.IsValid())
		{
			GEngine->OnLevelActorDeleted().Remove(W.DeletedHandle);
		}
		if (W.PropertyHandle.IsValid())
		{
			FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(W.PropertyHandle);
		}
		if (W.TickHandle.IsValid())
		{
			FTicker::GetCoreTicker().RemoveTicker(W.TickHandle);
		}
		delete W.Listener;
		W.Listener = nullptr;
		W.Table.Reset();
		W.TableHandle.Reset();
		W.MovedHandle.Reset();
		W.AddedHandle.Reset();
		W.DeletedHandle.Reset();
		W.PropertyHandle.Reset();
		W.TickHandle.Reset();
		W.StandIns.Reset();
	}

	void OvQueueCameraPreviewRefresh()
	{
		FOvCameraPreviewWatch& W = OvCamWatch();
		if (W.TickHandle.IsValid())
		{
			return;
		}
		W.TickHandle = FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
		{
			OvCamWatch().TickHandle.Reset();
			UWorld* World = OvEditorWorld();
			if (World && OvFindCameraPreview(World))
			{
				OvBuildCameraPreview(false);
			}
			else
			{
				OvUnwatchCameraPreview();
			}
			return false;
		}), 0.1f);
	}

	bool OvIsCameraPreviewInput(const AActor* A)
	{
		if (!A || A->ActorHasTag(CameraPreviewTag))
		{
			return false;
		}
		const FString ClassName = A->GetClass()->GetName();
		if (ClassName == TEXT("BP_Ring_C") || ClassName.StartsWith(TEXT("BP_StartMarker")) || ClassName == TEXT("PlayerStart"))
		{
			return true;
		}
		if (UClass* GameplayCameraClass = OvFindGameplayCameraClass())
		{
			if (A->IsA(GameplayCameraClass))
			{
				return true;
			}
		}
		return OvCamWatch().StandIns.ContainsByPredicate([A](const TWeakObjectPtr<AActor>& S) { return S.Get() == A; });
	}

	void OvWatchCameraPreview(UDataTable* Table)
	{
		FOvCameraPreviewWatch& W = OvCamWatch();
		if (W.Table.Get() != Table)
		{
			if (UDataTable* Old = W.Table.Get())
			{
				Old->OnDataTableChanged().Remove(W.TableHandle);
			}
			W.Table = Table;
			W.TableHandle = Table ? Table->OnDataTableChanged().AddLambda([]() { OvQueueCameraPreviewRefresh(); }) : FDelegateHandle();
		}
		if (!W.Listener)
		{
			W.Listener = new FOvCameraTableListener();
		}
		W.Listener->Table = Table;
		if (GEngine && !W.MovedHandle.IsValid())
		{
			W.MovedHandle = GEngine->OnActorMoved().AddLambda([](AActor* A)
			{
				if (OvIsCameraPreviewInput(A))
				{
					OvQueueCameraPreviewRefresh();
				}
			});
		}
		if (GEngine && !W.AddedHandle.IsValid())
		{
			auto RingChanged = [](AActor* A)
			{
				if (A && A->GetClass()->GetName() == TEXT("BP_Ring_C"))
				{
					OvQueueCameraPreviewRefresh();
				}
			};
			W.AddedHandle = GEngine->OnLevelActorAdded().AddLambda(RingChanged);
			W.DeletedHandle = GEngine->OnLevelActorDeleted().AddLambda(RingChanged);
		}
		if (!W.PropertyHandle.IsValid())
		{
			W.PropertyHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddLambda([](UObject* Object, FPropertyChangedEvent&)
			{
				if (!Object)
				{
					return;
				}
				FOvCameraPreviewWatch& Watch = OvCamWatch();
				const bool bTable = Object == Watch.Table.Get();
				const bool bSettings = Object == GetDefault<UAEWArenaMakerSettings>();
				const bool bOverride = Object->GetClass()->GetName() == TEXT("BP_CameraOverride_C");
				const UDataTable* AsTable = Cast<UDataTable>(Object);
				const bool bSameStruct = AsTable && Watch.Table.IsValid() && AsTable->RowStruct == Watch.Table->RowStruct;
				AActor* AsActor = Cast<AActor>(Object);
				if (bTable || bSettings || bOverride || bSameStruct || (AsActor && OvIsCameraPreviewInput(AsActor)))
				{
					OvQueueCameraPreviewRefresh();
				}
			});
		}
	}

	bool OvBuildCameraPreview(bool bInitial)
	{
		UWorld* World = OvEditorWorld();
		if (!World)
		{
			return false;
		}
		const UAEWArenaMakerSettings* Settings = OvOverrideSettings();

		bool bArenaTable = false;
		UDataTable* Table = OvCameraPreviewTable(World, bArenaTable);
		FOvCameraParams Cam;
		const FName RowName = Settings->CameraPreviewRow;
		if (!OvReadCameraRow(Table, RowName, Cam))
		{
			if (!bInitial)
			{
				return false;
			}
			OvReport(FString::Printf(TEXT("Row '%s' not found in %s.\n\nSet Camera Preview Row in Arena Maker settings."),
				*RowName.ToString(), Table ? *Table->GetName() : TEXT("the camera table")));
			return false;
		}

		TArray<FVector> Wrestlers;
		FString Source;
		FOvCameraPreviewWatch& Watch = OvCamWatch();
		if (bInitial)
		{
			Watch.StandIns.Reset();
			for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
			{
				AActor* A = Cast<AActor>(*It);
				if (A && !A->ActorHasTag(CameraPreviewTag) && !A->IsA<ACameraActor>())
				{
					Watch.StandIns.Add(A);
				}
			}
		}
		for (const TWeakObjectPtr<AActor>& A : Watch.StandIns)
		{
			if (A.IsValid())
			{
				Wrestlers.Add(A->GetActorLocation());
			}
		}
		if (Wrestlers.Num() > 0)
		{
			Source = FString::Printf(TEXT("%d selected actor(s)"), Wrestlers.Num());
		}
		else
		{
			const TCHAR* StartNames[] = { TEXT("PlayerStart_1on1_00"), TEXT("PlayerStart_1on1_01") };
			for (const TCHAR* Name : StartNames)
			{
				bool bFound = false;
				const FVector L = OvStartLocation(World, Name, bFound);
				if (bFound)
				{
					Wrestlers.Add(L);
				}
			}
			Source = TEXT("the 1-on-1 starts");
		}
		if (Wrestlers.Num() == 0)
		{
			if (bInitial)
			{
				OvReport(TEXT("No wrestler positions. Select stand-in actors or load ArenaMatch."));
			}
			return false;
		}

		AActor* Ring = nullptr;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->GetClass()->GetName() == TEXT("BP_Ring_C"))
			{
				Ring = *It;
				break;
			}
		}
		const bool bRingFound = Ring != nullptr;
		TArray<UPrimitiveComponent*> RingAreas;
		if (Ring)
		{
			TArray<UPrimitiveComponent*> Prims;
			Ring->GetComponents<UPrimitiveComponent>(Prims);
			for (UPrimitiveComponent* P : Prims)
			{
				if (P && P->GetName().StartsWith(TEXT("RingInArea")))
				{
					RingAreas.Add(P);
				}
			}
		}
		auto InRing = [&](const FVector& W)
		{
			if (!Ring)
			{
				return false;
			}
			if (RingAreas.Num() == 0)
			{
				const FVector L = Ring->GetActorTransform().InverseTransformPositionNoScale(W);
				return FMath::Abs(L.X) <= 300.f && FMath::Abs(L.Y) <= 300.f;
			}
			for (UPrimitiveComponent* Area : RingAreas)
			{
				if (const UBoxComponent* Box = Cast<UBoxComponent>(Area))
				{
					const FVector L = Box->GetComponentTransform().InverseTransformPosition(W);
					const FVector E = Box->GetUnscaledBoxExtent();
					if (FMath::Abs(L.X) <= E.X && FMath::Abs(L.Y) <= E.Y && FMath::Abs(L.Z) <= E.Z + 120.f)
					{
						return true;
					}
				}
				else if (Area->Bounds.GetBox().ExpandBy(FVector(0.f, 0.f, 120.f)).IsInside(W))
				{
					return true;
				}
			}
			return false;
		};
		bool bAllInRing = bRingFound;
		for (const FVector& W : Wrestlers)
		{
			bAllInRing &= InRing(W);
		}

		FVector Target = FVector::ZeroVector;
		for (const FVector& W : Wrestlers)
		{
			Target += W;
		}
		Target = Target / Wrestlers.Num() + Cam.TargetOffset;

		const float RMin = bAllInRing ? Cam.RMinRing : Cam.RMinArena;
		const float RMax = bAllInRing ? Cam.RMaxRing : Cam.RMaxArena;
		float Spread = 0.f;
		float RawSpread = 0.f;
		TArray<FVector> Points;
		TArray<FVector> Bones;
		for (const FVector& W : Wrestlers)
		{
			const FVector B[3] = { W + FVector(0.f, 0.f, 70.f), W + FVector(0.f, 20.f, -91.f), W + FVector(0.f, -20.f, -91.f) };
			for (const FVector& P : B)
			{
				Spread = FMath::Max(Spread, FVector::Dist(P, Target));
				RawSpread = FMath::Max(RawSpread, FVector::Dist(P, Target));
				Bones.Add(P);
			}
			Spread = Spread < RMin ? RMin : FMath::Min(Spread, RMax);
			for (const FVector& P : B)
			{
				Points.Add(OvClampFromTarget(Target, P, RMin, RMax));
			}
		}
		Points.Add(Target + FVector(0.f, 0.f, Spread * Cam.RateU));
		Points.Add(Target - FVector(0.f, 0.f, Spread * Cam.RateD));
		Points.Add(Target + FVector(Spread * Cam.RateX, 0.f, 0.f));
		Points.Add(Target - FVector(Spread * Cam.RateX, 0.f, 0.f));
		Points.Add(Target + FVector(0.f, Spread * Cam.RateY, 0.f));
		Points.Add(Target - FVector(0.f, Spread * Cam.RateY, 0.f));

		const float Alpha = (Spread - Cam.RMinRing) / FMath::Max(Cam.RMaxRing - Cam.RMinRing, 1.f);
		const float OffsetZ = Cam.OffZMin + Alpha * (Cam.OffZMax - Cam.OffZMin);

		const float Fov = 30.f;
		const float Aspect = 16.f / 9.f;
		FVector CamLoc(-2303.f, 0.f, 576.f);
		FRotator CamRot(-12.f, 0.f, 0.f);
		UClass* GameplayCameraClass = OvFindGameplayCameraClass();
		if (GameplayCameraClass)
		{
			for (TActorIterator<AActor> It(World, GameplayCameraClass); It; ++It)
			{
				CamLoc = It->GetActorLocation();
				CamRot = It->GetActorRotation();
				break;
			}
		}
		bool bClamped = false;
		float RawPitch = CamRot.Pitch;
		for (int32 i = 0; i < 300; ++i)
		{
			RawPitch = (Target - (CamLoc + FVector(0.f, 0.f, OffsetZ))).Rotation().Pitch;
			float Pitch = RawPitch;
			bClamped = false;
			if (Pitch < Cam.PitchMin)
			{
				Pitch = Cam.PitchMin;
				bClamped = true;
			}
			else if (Pitch >= Cam.PitchMax)
			{
				Pitch = Cam.PitchMax;
				bClamped = true;
			}
			CamRot = FRotator(Pitch, CamRot.Yaw, CamRot.Roll);
			CamLoc = OvFitPoints(Points, CamRot, Fov, Aspect, 20.f);
		}

		ACameraActor* Preview = OvFindCameraPreview(World);
		FlushPersistentDebugLines(World);
		const float Life = -1.f;
		DrawDebugSphere(World, Target, 15.f, 12, FColor::Yellow, true, Life, 0, 2.f);
		DrawDebugCircle(World, Target, Spread, 64, FColor::Yellow, true, Life, 0, 2.f, FVector(0.f, 1.f, 0.f), FVector(1.f, 0.f, 0.f), false);
		for (const FVector& W : Wrestlers)
		{
			DrawDebugCapsule(World, W, 88.f, 34.f, FQuat::Identity, FColor::Orange, true, Life, 0, 2.f);
		}
		for (const FVector& P : Points)
		{
			DrawDebugPoint(World, P, 14.f, FColor::Cyan, true, Life, 0);
		}
		DrawDebugCamera(World, CamLoc, CamRot, Fov, 3.f, FColor::Green, true, Life, 0);
		DrawDebugLine(World, CamLoc, Target, FColor::Green, true, Life, 0, 2.f);
		DrawDebugLine(World, CamLoc, FVector(CamLoc.X, CamLoc.Y, Target.Z - Cam.TargetOffset.Z - 88.f), FColor::Silver, true, Life, 0, 1.f);

		const bool bNewPreview = Preview == nullptr;
		if (bNewPreview)
		{
			FActorSpawnParameters Params;
			Params.OverrideLevel = World->PersistentLevel;
			Params.ObjectFlags |= RF_Transient;
			Preview = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), CamLoc, CamRot, Params);
		}
		else
		{
			Preview->SetActorLocationAndRotation(CamLoc, CamRot);
		}
		if (Preview)
		{
			Preview->Tags.AddUnique(CameraPreviewTag);
			Preview->SetActorLabel(FString::Printf(TEXT("GameplayCameraPreview_%s"), *RowName.ToString()));
			Preview->SetFolderPath(TEXT("ArenaOverrides"));
			if (UCameraComponent* C = Preview->GetCameraComponent())
			{
				C->SetFieldOfView(Fov);
				C->SetAspectRatio(Aspect);
				C->SetConstraintAspectRatio(true);
			}
			if (bNewPreview)
			{
				GEditor->SelectNone(false, true);
				GEditor->SelectActor(Preview, true, true);
			}
		}
		GEditor->RedrawLevelEditingViewports();
		OvWatchCameraPreview(Table);

		const float Ground = Wrestlers[0].Z - 99.f;
		const TCHAR* Radii = bAllInRing ? TEXT("Ring") : TEXT("Arena");
		const FString RingNote = !bRingFound ? FString(TEXT(" No BP_Ring, so the game uses the Arena radii."))
			: (bAllInRing ? FString(TEXT(" All wrestlers in the ring.")) : FString(TEXT(" Not all wrestlers in the ring.")));
		const FString PitchNote = bClamped
			? FString::Printf(TEXT(" (held at %s; raw %.1f, so CameraOffsetZ has no effect)"), RawPitch < Cam.PitchMin ? TEXT("Pitch_Min") : TEXT("Pitch_Max"), RawPitch)
			: FString();
		const FString SpreadNote = RawSpread < RMin ? FString::Printf(TEXT(" (raised to RadiusMin_%s)"), Radii)
			: (RawSpread > RMax ? FString::Printf(TEXT(" (capped at RadiusMax_%s)"), Radii)
			: FString::Printf(TEXT(" (between RadiusMin_%s %.0f and RadiusMax_%s %.0f, so they have no effect)"), Radii, RMin, Radii, RMax));
		const FString Summary = FString::Printf(TEXT("Pitch %.1f%s.\nSpread %.0f%s.%s\nCamera %.0f from target, %.0f above feet."),
			CamRot.Pitch, *PitchNote, Spread, *SpreadNote, *RingNote, FVector::Dist(CamLoc, Target), CamLoc.Z - Ground);
		if (!bInitial)
		{
			if (TSharedPtr<SNotificationItem> Old = Watch.Toast.Pin())
			{
				Old->SetFadeOutDuration(0.f);
				Old->Fadeout();
			}
			FNotificationInfo Info(FText::FromString(FString::Printf(TEXT("Camera preview updated: %s"), *RowName.ToString())));
			Info.ExpireDuration = 2.f;
			Info.bFireAndForget = true;
			Watch.Toast = FSlateNotificationManager::Get().AddNotification(Info);
			return true;
		}

		FString Message = FString::Printf(TEXT("Row %s of %s (%s), wrestlers at %s.\n\n"),
			*RowName.ToString(), *Table->GetName(), bArenaTable ? TEXT("arena table") : TEXT("game table"), *Source);
		Message += Summary + TEXT("\n\n");
		Message += TEXT("Green: camera. Yellow: target. Cyan: framed points. Orange: wrestlers.\n");
		Message += TEXT("Updates live. Select actors first to move the wrestlers. Run again to remove.");
		OvReport(Message);
		return true;
	}
}

bool UAEWArenaTools::PreviewGameplayCamera()
{
	return OvBuildCameraPreview(true);
}

TArray<FName> UAEWArenaTools::CameraPreviewRows()
{
	TArray<FName> Rows;
	if (UWorld* World = OvEditorWorld())
	{
		bool bArenaTable = false;
		if (UDataTable* Table = OvCameraPreviewTable(World, bArenaTable))
		{
			Rows = Table->GetRowNames();
		}
	}
	return Rows;
}

void UAEWArenaTools::SetCameraPreviewRow(FName Row)
{
	UAEWArenaMakerSettings* S = GetMutableDefault<UAEWArenaMakerSettings>();
	S->CameraPreviewRow = Row;
	S->SaveConfig();
	S->UpdateDefaultConfigFile();
	UWorld* World = OvEditorWorld();
	if (World && OvFindCameraPreview(World))
	{
		OvBuildCameraPreview(false);
	}
	else
	{
		OvBuildCameraPreview(true);
	}
}

namespace
{
	const TCHAR* EntranceOverrideName = TEXT("BP_EntranceOverride");
	const FName DefaultEntranceVar(TEXT("DefaultEntranceTransform"));
	const FName MoveManagerVar(TEXT("bMoveEntranceManager"));
	const FName LastManagerVar(TEXT("LastManagerTransform"));
	const FName ActiveVar(TEXT("bEntranceActive"));
	const FName TrackedCharsVar(TEXT("TrackedCharacters"));
	const FName RootMatchesVar(TEXT("bRootMatches"));
	const FName SequenceListVar(TEXT("SequenceList"));
	const FName ExclusionVar(TEXT("bListIsExclusion"));
	const TCHAR* CrowdSequences = TEXT("LS_000_00008_A_Ent_JonMoxley,LS_000_00604_A_Ent_Title_JonMoxley");
	const TCHAR* RingSequences = TEXT("LS_000_00059_A_Ent_AdamCole-v2");
	const FName ForceStageVar(TEXT("bForceStageEntrances"));
	const FName SwapFromVar(TEXT("SwapFromEntrances"));
	const FName SwapToVar(TEXT("SwapToEntrances"));

	const FName TrackedLocsVar(TEXT("TrackedLocations"));

	UK2Node_CallArrayFunction* OvArrayCall(UEdGraph* Graph, const TCHAR* Function, int32 X, int32 Y)
	{
		UFunction* F = UKismetArrayLibrary::StaticClass()->FindFunctionByName(Function);
		if (!F)
		{
			UE_LOG(LogAEWArenaOverrides, Error, TEXT("function KismetArrayLibrary::%s not found"), Function);
			return nullptr;
		}
		UK2Node_CallArrayFunction* Node = NewObject<UK2Node_CallArrayFunction>(Graph);
		Node->SetFromFunction(F);
		return OvPlace(Graph, Node, X, Y);
	}

	const FVector DefaultEntranceLocation(2437.f, 0.f, 87.f);

	UClass* OvFindNativeClass(const TCHAR* Name)
	{
		for (TObjectIterator<UClass> It; It; ++It)
		{
			if (It->GetFName() == Name && It->IsNative())
			{
				return *It;
			}
		}
		return nullptr;
	}

	UK2Node_VariableGet* OvGetMember(UEdGraph* Graph, UClass* Class, const FName Name, int32 X, int32 Y)
	{
		UK2Node_VariableGet* Node = NewObject<UK2Node_VariableGet>(Graph);
		Node->VariableReference.SetExternalMember(Name, Class);
		return OvPlace(Graph, Node, X, Y);
	}

	UK2Node_VariableSet* OvSetMember(UEdGraph* Graph, UClass* Class, const FName Name, int32 X, int32 Y)
	{
		UK2Node_VariableSet* Node = NewObject<UK2Node_VariableSet>(Graph);
		Node->VariableReference.SetExternalMember(Name, Class);
		return OvPlace(Graph, Node, X, Y);
	}

	UK2Node_VariableSet* OvSetSelfVar(UEdGraph* Graph, const FName Name, int32 X, int32 Y)
	{
		UK2Node_VariableSet* Node = NewObject<UK2Node_VariableSet>(Graph);
		Node->VariableReference.SetSelfMember(Name);
		return OvPlace(Graph, Node, X, Y);
	}

	UEdGraphPin* OvOriginPin(UEdGraph* G, int32 X, int32 Y)
	{
		UK2Node_VariableGet* Default = OvGetSelfVar(G, DefaultEntranceVar, X, Y);
		UK2Node_CallFunction* Invert = OvCall(G, UKismetMathLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, InvertTransform), X + 250, Y);
		UK2Node_CallFunction* Mine = OvCall(G, AActor::StaticClass(), GET_FUNCTION_NAME_CHECKED(AActor, GetTransform), X + 250, Y + 120);
		UK2Node_CallFunction* Compose = OvCall(G, UKismetMathLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, ComposeTransforms), X + 500, Y);
		if (!Invert || !Mine || !Compose)
		{
			return nullptr;
		}
		OvLink(OvPin(Default, DefaultEntranceVar, EGPD_Output), OvPin(Invert, TEXT("T"), EGPD_Input));
		OvLink(OvOut(Invert), OvPin(Compose, TEXT("A"), EGPD_Input));
		OvLink(OvOut(Mine), OvPin(Compose, TEXT("B"), EGPD_Input));
		return OvOut(Compose);
	}
}

UBlueprint* UAEWArenaTools::EnsureEntranceOverrideBlueprint(const FString& ArenaFolder, bool bRebuild)
{
	UClass* SequenceActorClass = FindObject<UClass>(nullptr, TEXT("/Script/LevelSequence.LevelSequenceActor"));
	UClass* InstanceDataClass = FindObject<UClass>(nullptr, TEXT("/Script/LevelSequence.DefaultLevelSequenceInstanceData"));
	UClass* ManagerClass = OvFindNativeClass(TEXT("ELEntranceCutsceneManager"));
	UClass* CharacterClass = OvFindNativeClass(TEXT("ELCharacter_Native"));
	UClass* PlayerClass = FindObject<UClass>(nullptr, TEXT("/Script/MovieScene.MovieSceneSequencePlayer"));
	if (!SequenceActorClass || !InstanceDataClass || !ManagerClass || !CharacterClass || !PlayerClass)
	{
		UE_LOG(LogAEWArenaOverrides, Error, TEXT("LevelSequenceActor, DefaultLevelSequenceInstanceData or ELEntranceCutsceneManager not found."));
		return nullptr;
	}
	bool bNew = false;
	UBlueprint* BP = OvNewActorBlueprint(ArenaFolder, EntranceOverrideName, bRebuild, bNew);
	if (!BP || (!bNew && !bRebuild))
	{
		return BP;
	}

	if (bNew)
	{
		USCS_Node* Root = OvAddComponent(BP, USceneComponent::StaticClass(), TEXT("Root"), nullptr);
		OvAddComponent(BP, UBillboardComponent::StaticClass(), TEXT("Icon"), Root);
		USCS_Node* ArrowNode = OvAddComponent(BP, UArrowComponent::StaticClass(), TEXT("Facing"), Root);
		if (UArrowComponent* Arrow = Cast<UArrowComponent>(ArrowNode->ComponentTemplate))
		{
			Arrow->ArrowColor = FColor(255, 0, 200);
			Arrow->ArrowSize = 3.f;
			Arrow->SetHiddenInGame(true);
		}
		FEdGraphPinType TransformType;
		TransformType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		TransformType.PinSubCategoryObject = TBaseStructure<FTransform>::Get();
		FBlueprintEditorUtils::AddMemberVariable(BP, DefaultEntranceVar, TransformType, FString::Printf(TEXT("%f,%f,%f|0.000000,0.000000,0.000000|1.000000,1.000000,1.000000"), DefaultEntranceLocation.X, DefaultEntranceLocation.Y, DefaultEntranceLocation.Z));
		FBlueprintEditorUtils::SetBlueprintOnlyEditableFlag(BP, DefaultEntranceVar, false);
		FBlueprintEditorUtils::SetBlueprintVariableMetaData(BP, DefaultEntranceVar, nullptr, TEXT("tooltip"),
			TEXT("Default entrance staging spot. Entrances move by the offset from here to this actor."));
		FBlueprintEditorUtils::SetBlueprintVariableCategory(BP, DefaultEntranceVar, nullptr, FText::FromString(TEXT("Arena Override")));
		FEdGraphPinType BoolType;
		BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
		FBlueprintEditorUtils::AddMemberVariable(BP, MoveManagerVar, BoolType, TEXT("true"));
		FBlueprintEditorUtils::SetBlueprintOnlyEditableFlag(BP, MoveManagerVar, false);
		FBlueprintEditorUtils::SetBlueprintVariableMetaData(BP, MoveManagerVar, nullptr, TEXT("tooltip"),
			TEXT("Also move the entrance manager by the same amount."));
		FBlueprintEditorUtils::SetBlueprintVariableCategory(BP, MoveManagerVar, nullptr, FText::FromString(TEXT("Arena Override")));
		FBlueprintEditorUtils::AddMemberVariable(BP, LastManagerVar, TransformType);
	}

	if (FBlueprintEditorUtils::FindNewVariableIndex(BP, ActiveVar) == INDEX_NONE)
	{
		FEdGraphPinType BoolType;
		BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
		FBlueprintEditorUtils::AddMemberVariable(BP, ActiveVar, BoolType);
	}
	if (FBlueprintEditorUtils::FindNewVariableIndex(BP, SequenceListVar) == INDEX_NONE)
	{
		FEdGraphPinType StrType;
		StrType.PinCategory = UEdGraphSchema_K2::PC_String;
		FBlueprintEditorUtils::AddMemberVariable(BP, SequenceListVar, StrType, FString::Printf(TEXT("%s,%s"), CrowdSequences, RingSequences));
		FBlueprintEditorUtils::SetBlueprintOnlyEditableFlag(BP, SequenceListVar, false);
		FBlueprintEditorUtils::SetBlueprintVariableMetaData(BP, SequenceListVar, nullptr, TEXT("tooltip"),
			TEXT("Comma-separated entrance sequences, e.g. LS_000_00008_A_Ent_JonMoxley."));
		FBlueprintEditorUtils::SetBlueprintVariableCategory(BP, SequenceListVar, nullptr, FText::FromString(TEXT("Arena Override")));
	}
	if (FBlueprintEditorUtils::FindNewVariableIndex(BP, ExclusionVar) == INDEX_NONE)
	{
		FEdGraphPinType BoolType;
		BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
		FBlueprintEditorUtils::AddMemberVariable(BP, ExclusionVar, BoolType, TEXT("true"));
		FBlueprintEditorUtils::SetBlueprintOnlyEditableFlag(BP, ExclusionVar, false);
		FBlueprintEditorUtils::SetBlueprintVariableMetaData(BP, ExclusionVar, nullptr, TEXT("tooltip"),
			TEXT("On: every entrance except Sequence List. Off: only Sequence List."));
		FBlueprintEditorUtils::SetBlueprintVariableCategory(BP, ExclusionVar, nullptr, FText::FromString(TEXT("Arena Override")));
	}
	if (FBlueprintEditorUtils::FindNewVariableIndex(BP, ForceStageVar) == INDEX_NONE)
	{
		FEdGraphPinType BoolType;
		BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
		FBlueprintEditorUtils::AddMemberVariable(BP, ForceStageVar, BoolType, TEXT("false"));
		FBlueprintEditorUtils::SetBlueprintOnlyEditableFlag(BP, ForceStageVar, false);
		FBlueprintEditorUtils::SetBlueprintVariableMetaData(BP, ForceStageVar, nullptr, TEXT("tooltip"),
			TEXT("Swap Swap From Entrances for Swap To Entrances in this arena. Written on map save."));
		FBlueprintEditorUtils::SetBlueprintVariableCategory(BP, ForceStageVar, nullptr, FText::FromString(TEXT("Arena Override")));
	}
	const FName SwapVars[] = { SwapFromVar, SwapToVar };
	const TCHAR* SwapDefaults[] = { TEXT("(40,2003,54)"), TEXT("(104,104,50)") };
	for (int32 SwapIndex = 0; SwapIndex < 2; ++SwapIndex)
	{
		const FName Key = SwapVars[SwapIndex];
		if (FBlueprintEditorUtils::FindNewVariableIndex(BP, Key) == INDEX_NONE)
		{
			FEdGraphPinType IntArray;
			IntArray.PinCategory = UEdGraphSchema_K2::PC_Int;
			IntArray.ContainerType = EPinContainerType::Array;
			FBlueprintEditorUtils::AddMemberVariable(BP, Key, IntArray, SwapDefaults[SwapIndex]);
			FBlueprintEditorUtils::SetBlueprintOnlyEditableFlag(BP, Key, false);
			FBlueprintEditorUtils::SetBlueprintVariableMetaData(BP, Key, nullptr, TEXT("tooltip"),
				TEXT("Entrance IDs (DT_ELEntranceCutsceneAnimationData rows), e.g. 40, 104."));
			FBlueprintEditorUtils::SetBlueprintVariableCategory(BP, Key, nullptr, FText::FromString(TEXT("Arena Override")));
		}
	}
	if (FBlueprintEditorUtils::FindNewVariableIndex(BP, RootMatchesVar) == INDEX_NONE)
	{
		FEdGraphPinType BoolType;
		BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
		FBlueprintEditorUtils::AddMemberVariable(BP, RootMatchesVar, BoolType);
	}
	if (FBlueprintEditorUtils::FindNewVariableIndex(BP, TrackedCharsVar) == INDEX_NONE)
	{
		FEdGraphPinType ArrayType;
		ArrayType.PinCategory = UEdGraphSchema_K2::PC_Object;
		ArrayType.PinSubCategoryObject = AActor::StaticClass();
		ArrayType.ContainerType = EPinContainerType::Array;
		FBlueprintEditorUtils::AddMemberVariable(BP, TrackedCharsVar, ArrayType);
	}
	if (FBlueprintEditorUtils::FindNewVariableIndex(BP, TrackedLocsVar) == INDEX_NONE)
	{
		FEdGraphPinType ArrayType;
		ArrayType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		ArrayType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
		ArrayType.ContainerType = EPinContainerType::Array;
		FBlueprintEditorUtils::AddMemberVariable(BP, TrackedLocsVar, ArrayType);
	}

	UEdGraph* G = FBlueprintEditorUtils::FindEventGraph(BP);
	UK2Node_Event* Tick = NewObject<UK2Node_Event>(G);
	Tick->EventReference.SetExternalMember(TEXT("ReceiveTick"), AActor::StaticClass());
	Tick->bOverrideFunction = true;
	OvPlace(G, Tick, 0, 0);

	UK2Node_VariableSet* ClearActive = OvSetSelfVar(G, ActiveVar, 150, -200);
	UK2Node_CallFunction* GetAll = OvCall(G, UGameplayStatics::StaticClass(), GET_FUNCTION_NAME_CHECKED(UGameplayStatics, GetAllActorsOfClass), 250, 0);
	OvSetObject(OvPin(GetAll, TEXT("ActorClass"), EGPD_Input), SequenceActorClass);
	UK2Node_MacroInstance* Loop = NewObject<UK2Node_MacroInstance>(G);
	Loop->SetMacroGraph(OvForEachMacro());
	OvPlace(G, Loop, 550, 0);
	UK2Node_VariableGet* SeqPath = OvGetMember(G, SequenceActorClass, TEXT("LevelSequence"), 550, 250);
	UK2Node_CallFunction* Break = OvCall(G, UKismetSystemLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, BreakSoftObjectPath), 800, 250);
	UK2Node_CallFunction* Contains = OvCall(G, UKismetStringLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetStringLibrary, Contains), 1050, 250);
	OvSetValue(OvPin(Contains, TEXT("Substring"), EGPD_Input), TEXT("/Entranse/"));
	UK2Node_CallFunction* ContainsEnt = OvCall(G, UKismetStringLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetStringLibrary, Contains), 1050, 400);
	OvSetValue(OvPin(ContainsEnt, TEXT("Substring"), EGPD_Input), TEXT("_Ent_"));
	UK2Node_CallFunction* AnyEntrance = OvCall(G, UKismetMathLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, BooleanOR), 1200, 300);
	UK2Node_CallFunction* GetSeq = OvCall(G, SequenceActorClass, TEXT("GetSequence"), 550, 550);
	UK2Node_CallFunction* SeqName = OvCall(G, UKismetSystemLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, GetObjectName), 750, 550);
	UK2Node_VariableGet* ListGet = OvGetSelfVar(G, SequenceListVar, 550, 650);
	UK2Node_CallFunction* WrapList1 = OvCall(G, UKismetStringLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetStringLibrary, Concat_StrStr), 750, 650);
	OvSetValue(OvPin(WrapList1, TEXT("A"), EGPD_Input), TEXT(","));
	UK2Node_CallFunction* WrapList2 = OvCall(G, UKismetStringLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetStringLibrary, Concat_StrStr), 950, 650);
	OvSetValue(OvPin(WrapList2, TEXT("B"), EGPD_Input), TEXT(","));
	UK2Node_CallFunction* WrapName1 = OvCall(G, UKismetStringLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetStringLibrary, Concat_StrStr), 950, 550);
	OvSetValue(OvPin(WrapName1, TEXT("A"), EGPD_Input), TEXT(","));
	UK2Node_CallFunction* WrapName2 = OvCall(G, UKismetStringLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetStringLibrary, Concat_StrStr), 1150, 550);
	OvSetValue(OvPin(WrapName2, TEXT("B"), EGPD_Input), TEXT(","));
	UK2Node_CallFunction* Listed = OvCall(G, UKismetStringLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetStringLibrary, Contains), 1150, 650);
	UK2Node_VariableGet* ExclGet = OvGetSelfVar(G, ExclusionVar, 1150, 750);
	UK2Node_CallFunction* Claim = OvCall(G, UKismetMathLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, NotEqual_BoolBool), 1350, 650);
	UK2Node_CallFunction* Mine = OvCall(G, UKismetMathLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, BooleanAND), 1250, 350);
	UK2Node_IfThenElse* IsSequence = OvBranch(G, 1150, -150);
	UK2Node_VariableGet* GetData2 = OvGetMember(G, SequenceActorClass, TEXT("DefaultInstanceData"), 1550, -300);
	UK2Node_DynamicCast* CastData2 = NewObject<UK2Node_DynamicCast>(G);
	CastData2->TargetType = InstanceDataClass;
	OvPlace(G, CastData2, 1850, -300);
	UK2Node_VariableGet* CurrentOrigin = OvGetMember(G, InstanceDataClass, TEXT("TransformOrigin"), 2100, -150);
	UEdGraphPin* Origin4 = OvOriginPin(G, 1850, -500);
	UK2Node_CallFunction* IsOurs = OvCall(G, UKismetMathLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, EqualEqual_TransformTransform), 2400, -200);
	UK2Node_IfThenElse* WasOurs = OvBranch(G, 2600, -300);
	UK2Node_VariableSet* Release = OvSetMember(G, SequenceActorClass, TEXT("bOverrideInstanceData"), 2850, -300);
	UK2Node_IfThenElse* IsEntrance = OvBranch(G, 1300, 0);
	UK2Node_VariableSet* SetOverride = OvSetMember(G, SequenceActorClass, TEXT("bOverrideInstanceData"), 1550, 0);
	OvSetValue(OvPin(SetOverride, TEXT("bOverrideInstanceData"), EGPD_Input), TEXT("true"));
	UK2Node_VariableGet* GetData = OvGetMember(G, SequenceActorClass, TEXT("DefaultInstanceData"), 1550, 250);
	UK2Node_DynamicCast* CastData = NewObject<UK2Node_DynamicCast>(G);
	CastData->TargetType = InstanceDataClass;
	OvPlace(G, CastData, 1850, 0);
	UK2Node_VariableSet* SetOriginActor = OvSetMember(G, InstanceDataClass, TEXT("TransformOriginActor"), 2150, 0);
	UK2Node_VariableSet* SetOrigin = OvSetMember(G, InstanceDataClass, TEXT("TransformOrigin"), 2450, 0);
	UEdGraphPin* Origin1 = OvOriginPin(G, 1850, 300);
	UK2Node_VariableGet* SeqPlayer = OvGetMember(G, SequenceActorClass, TEXT("SequencePlayer"), 2450, 250);
	UK2Node_CallFunction* Playing = OvCall(G, PlayerClass, TEXT("IsPlaying"), 2700, 250);
	UK2Node_IfThenElse* IsPlaying = OvBranch(G, 2750, 0);
	UK2Node_VariableSet* SetActive = OvSetSelfVar(G, ActiveVar, 3000, 0);
	OvSetValue(OvPin(SetActive, ActiveVar, EGPD_Input), TEXT("true"));

	UK2Node_VariableGet* ActiveGet = OvGetSelfVar(G, ActiveVar, 250, 500);
	UK2Node_IfThenElse* DuringEntrance = OvBranch(G, 300, 450);
	UK2Node_CallFunction* GetChars = OvCall(G, UGameplayStatics::StaticClass(), GET_FUNCTION_NAME_CHECKED(UGameplayStatics, GetAllActorsOfClass), 550, 450);
	OvSetObject(OvPin(GetChars, TEXT("ActorClass"), EGPD_Input), CharacterClass);
	UK2Node_MacroInstance* CharLoop = NewObject<UK2Node_MacroInstance>(G);
	CharLoop->SetMacroGraph(OvForEachMacro());
	OvPlace(G, CharLoop, 850, 450);
	UK2Node_CallFunction* CharLoc = OvCall(G, AActor::StaticClass(), GET_FUNCTION_NAME_CHECKED(AActor, K2_GetActorLocation), 850, 600);
	UK2Node_VariableGet* DefaultGet = OvGetSelfVar(G, DefaultEntranceVar, 850, 700);
	UK2Node_CallFunction* BreakDefault = OvCall(G, UKismetMathLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, BreakTransform), 1050, 700);
	UK2Node_CallFunction* DistDefault = OvCall(G, UKismetMathLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Vector_Distance), 1300, 600);
	UK2Node_CallFunction* NearDefault = OvCall(G, UKismetMathLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Less_FloatFloat), 1500, 600);
	OvSetValue(OvPin(NearDefault, TEXT("B"), EGPD_Input), TEXT("600.0"));
	UK2Node_CallArrayFunction* FindIdx = OvArrayCall(G, TEXT("Array_Find"), 1100, 900);
	UK2Node_VariableGet* CharsGet1 = OvGetSelfVar(G, TrackedCharsVar, 900, 900);
	UK2Node_CallFunction* IsNew = OvCall(G, UKismetMathLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, EqualEqual_IntInt), 1300, 900);
	OvSetValue(OvPin(IsNew, TEXT("B"), EGPD_Input), TEXT("-1"));
	UK2Node_IfThenElse* NewBranch = OvBranch(G, 1500, 450);
	UK2Node_CallArrayFunction* AddChar = OvArrayCall(G, TEXT("Array_Add"), 1750, 300);
	UK2Node_VariableGet* CharsGet2 = OvGetSelfVar(G, TrackedCharsVar, 1550, 300);
	UK2Node_CallArrayFunction* AddLoc = OvArrayCall(G, TEXT("Array_Add"), 2000, 300);
	UK2Node_VariableGet* LocsGet1 = OvGetSelfVar(G, TrackedLocsVar, 1800, 200);
	UK2Node_IfThenElse* NewNear = OvBranch(G, 2250, 300);
	UK2Node_GetArrayItem* Prev = NewObject<UK2Node_GetArrayItem>(G);
	OvPlace(G, Prev, 1550, 1050);
	UK2Node_VariableGet* LocsGet2 = OvGetSelfVar(G, TrackedLocsVar, 1350, 1050);
	UK2Node_CallFunction* DistPrev = OvCall(G, UKismetMathLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Vector_Distance), 1750, 1000);
	UK2Node_CallFunction* Jumped = OvCall(G, UKismetMathLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Greater_FloatFloat), 1950, 1000);
	OvSetValue(OvPin(Jumped, TEXT("B"), EGPD_Input), TEXT("150.0"));
	UK2Node_CallFunction* Both = OvCall(G, UKismetMathLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, BooleanAND), 2150, 950);
	UK2Node_IfThenElse* AtDefault = OvBranch(G, 2250, 600);
	UK2Node_CallArrayFunction* Update = OvArrayCall(G, TEXT("Array_Set"), 3050, 600);
	OvSetValue(OvPin(Update, TEXT("bSizeToFit"), EGPD_Input), TEXT("true"));
	UK2Node_VariableGet* LocsGet3 = OvGetSelfVar(G, TrackedLocsVar, 2850, 750);
	UK2Node_CallArrayFunction* ClearChars = OvArrayCall(G, TEXT("Array_Clear"), 550, 1200);
	UK2Node_VariableGet* CharsGet3 = OvGetSelfVar(G, TrackedCharsVar, 350, 1300);
	UK2Node_CallArrayFunction* ClearLocs = OvArrayCall(G, TEXT("Array_Clear"), 800, 1200);
	UK2Node_VariableGet* LocsGet4 = OvGetSelfVar(G, TrackedLocsVar, 600, 1300);
	UK2Node_CallFunction* CharTransform = OvCall(G, AActor::StaticClass(), GET_FUNCTION_NAME_CHECKED(AActor, GetTransform), 1900, 650);
	UEdGraphPin* Origin3 = OvOriginPin(G, 1900, 800);
	UK2Node_CallFunction* CharCompose = OvCall(G, UKismetMathLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, ComposeTransforms), 2500, 650);
	UK2Node_CallFunction* CharPlace = OvCall(G, AActor::StaticClass(), GET_FUNCTION_NAME_CHECKED(AActor, K2_SetActorTransform), 2750, 450);
	OvSetValue(OvPin(CharPlace, TEXT("bTeleport"), EGPD_Input), TEXT("true"));

	UK2Node_CallFunction* FindManager = OvCall(G, UGameplayStatics::StaticClass(), GET_FUNCTION_NAME_CHECKED(UGameplayStatics, GetActorOfClass), 550, 700);
	OvSetObject(OvPin(FindManager, TEXT("ActorClass"), EGPD_Input), ManagerClass);
	UK2Node_CallFunction* Valid = OvCall(G, UKismetSystemLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, IsValid), 800, 850);
	UK2Node_VariableGet* MoveFlag = OvGetSelfVar(G, MoveManagerVar, 800, 950);
	UK2Node_IfThenElse* HasManager = OvBranch(G, 1300, 700);
	UK2Node_CallFunction* Observed = OvCall(G, AActor::StaticClass(), GET_FUNCTION_NAME_CHECKED(AActor, GetTransform), 1300, 950);
	UK2Node_VariableGet* Last = OvGetSelfVar(G, LastManagerVar, 1300, 1080);
	UK2Node_CallFunction* Same = OvCall(G, UKismetMathLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, EqualEqual_TransformTransform), 1550, 950);
	UK2Node_IfThenElse* Moved = OvBranch(G, 1800, 700);
	UEdGraphPin* Origin2 = OvOriginPin(G, 1800, 1100);
	UK2Node_CallFunction* Compose = OvCall(G, UKismetMathLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, ComposeTransforms), 2350, 950);
	UK2Node_CallFunction* Place = OvCall(G, AActor::StaticClass(), GET_FUNCTION_NAME_CHECKED(AActor, K2_SetActorTransform), 2600, 700);
	OvSetValue(OvPin(Place, TEXT("bTeleport"), EGPD_Input), TEXT("true"));
	UK2Node_CallFunction* After = OvCall(G, AActor::StaticClass(), GET_FUNCTION_NAME_CHECKED(AActor, GetTransform), 2900, 950);
	UK2Node_VariableSet* Remember = OvSetSelfVar(G, LastManagerVar, 3150, 700);
	UK2Node_VariableGet* DefaultGet2 = OvGetSelfVar(G, DefaultEntranceVar, 1550, 1250);
	UK2Node_CallFunction* BreakDefault2 = OvCall(G, UKismetMathLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, BreakTransform), 1750, 1250);
	UK2Node_CallFunction* BreakObserved = OvCall(G, UKismetMathLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, BreakTransform), 1750, 1100);
	UK2Node_CallFunction* RootDist = OvCall(G, UKismetMathLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Vector_Distance), 2000, 1150);
	UK2Node_CallFunction* NearRoot = OvCall(G, UKismetMathLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Less_FloatFloat), 2200, 1150);
	OvSetValue(OvPin(NearRoot, TEXT("B"), EGPD_Input), TEXT("100.0"));
	UK2Node_VariableSet* SetMatch = OvSetSelfVar(G, RootMatchesVar, 2050, 700);
	UK2Node_CallFunction* MoveAnd = OvCall(G, UKismetMathLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, BooleanAND), 2250, 850);
	UK2Node_IfThenElse* ShouldMove = OvBranch(G, 2350, 700);

	if (!Playing || !GetChars || !CharLoop->GetMacroGraph() || !CharLoc || !BreakDefault || !DistDefault || !NearDefault || !FindIdx || !IsNew || !AddChar || !AddLoc || !DistPrev || !Jumped || !Both || !Update || !ClearChars || !ClearLocs || !CharTransform || !Origin3 || !CharCompose || !CharPlace)
	{
		return nullptr;
	}
	if (!GetAll || !Loop->GetMacroGraph() || !Break || !Contains || !ContainsEnt || !AnyEntrance || !Mine || !Origin4 || !IsOurs || !GetSeq || !SeqName || !WrapList1 || !WrapList2 || !WrapName1 || !WrapName2 || !Listed || !Claim || !Origin1 || !Origin2 || !FindManager || !Valid || !BreakDefault2 || !BreakObserved || !RootDist || !NearRoot || !MoveAnd || !Observed || !Same || !Compose || !Place || !After)
	{
		return nullptr;
	}

	OvLink(OvThen(Tick), OvExec(ClearActive));
	OvLink(OvThen(ClearActive), OvExec(GetAll));
	OvLink(OvThen(GetAll), OvPin(Loop, TEXT("Exec"), EGPD_Input));
	OvLink(OvPin(GetAll, TEXT("OutActors"), EGPD_Output), OvPin(Loop, TEXT("Array"), EGPD_Input));
	UEdGraphPin* Element = OvPin(Loop, TEXT("Array Element"), EGPD_Output);
	OvLink(OvLoopBody(Loop), OvExec(IsEntrance));
	OvLink(Element, OvTarget(SeqPath));
	OvLink(OvPin(SeqPath, TEXT("LevelSequence"), EGPD_Output), OvPin(Break, TEXT("InSoftObjectPath"), EGPD_Input));
	OvLink(OvPin(Break, TEXT("PathString"), EGPD_Output), OvPin(Contains, TEXT("SearchIn"), EGPD_Input));
	OvLink(OvPin(Break, TEXT("PathString"), EGPD_Output), OvPin(ContainsEnt, TEXT("SearchIn"), EGPD_Input));
	OvLink(OvOut(Contains), OvPin(AnyEntrance, TEXT("A"), EGPD_Input));
	OvLink(OvOut(ContainsEnt), OvPin(AnyEntrance, TEXT("B"), EGPD_Input));
	OvLink(Element, OvTarget(GetSeq));
	OvLink(OvOut(GetSeq), OvPin(SeqName, TEXT("Object"), EGPD_Input));
	OvLink(OvOut(SeqName), OvPin(WrapName1, TEXT("B"), EGPD_Input));
	OvLink(OvOut(WrapName1), OvPin(WrapName2, TEXT("A"), EGPD_Input));
	OvLink(OvPin(ListGet, SequenceListVar, EGPD_Output), OvPin(WrapList1, TEXT("B"), EGPD_Input));
	OvLink(OvOut(WrapList1), OvPin(WrapList2, TEXT("A"), EGPD_Input));
	OvLink(OvOut(WrapList2), OvPin(Listed, TEXT("SearchIn"), EGPD_Input));
	OvLink(OvOut(WrapName2), OvPin(Listed, TEXT("Substring"), EGPD_Input));
	OvLink(OvOut(Listed), OvPin(Claim, TEXT("A"), EGPD_Input));
	OvLink(OvPin(ExclGet, ExclusionVar, EGPD_Output), OvPin(Claim, TEXT("B"), EGPD_Input));
	OvLink(OvOut(Claim), OvPin(Mine, TEXT("A"), EGPD_Input));
	OvLink(OvOut(AnyEntrance), OvPin(Mine, TEXT("B"), EGPD_Input));
	OvLink(OvOut(Mine), IsEntrance->GetConditionPin());
	OvLink(IsEntrance->GetElsePin(), OvExec(IsSequence));
	OvLink(OvOut(AnyEntrance), IsSequence->GetConditionPin());
	OvLink(IsSequence->GetThenPin(), OvExec(CastData2));
	OvLink(Element, OvTarget(GetData2));
	OvLink(OvPin(GetData2, TEXT("DefaultInstanceData"), EGPD_Output), CastData2->GetCastSourcePin());
	OvLink(CastData2->GetCastResultPin(), OvTarget(CurrentOrigin));
	OvLink(OvPin(CurrentOrigin, TEXT("TransformOrigin"), EGPD_Output), OvPin(IsOurs, TEXT("A"), EGPD_Input));
	OvLink(Origin4, OvPin(IsOurs, TEXT("B"), EGPD_Input));
	OvLink(CastData2->GetValidCastPin(), OvExec(WasOurs));
	OvLink(OvOut(IsOurs), WasOurs->GetConditionPin());
	OvLink(WasOurs->GetThenPin(), OvExec(Release));
	OvLink(Element, OvTarget(Release));
	OvLink(IsEntrance->GetThenPin(), OvExec(SetOverride));
	OvLink(Element, OvTarget(SetOverride));
	OvLink(Element, OvTarget(GetData));
	OvLink(OvThen(SetOverride), OvExec(CastData));
	OvLink(OvPin(GetData, TEXT("DefaultInstanceData"), EGPD_Output), CastData->GetCastSourcePin());
	OvLink(CastData->GetValidCastPin(), OvExec(SetOriginActor));
	OvLink(CastData->GetCastResultPin(), OvTarget(SetOriginActor));
	OvLink(OvThen(SetOriginActor), OvExec(SetOrigin));
	OvLink(CastData->GetCastResultPin(), OvTarget(SetOrigin));
	OvLink(Origin1, OvPin(SetOrigin, TEXT("TransformOrigin"), EGPD_Input));
	OvLink(OvThen(SetOrigin), OvExec(IsPlaying));
	OvLink(Element, OvTarget(SeqPlayer));
	OvLink(OvPin(SeqPlayer, TEXT("SequencePlayer"), EGPD_Output), OvTarget(Playing));
	OvLink(OvOut(Playing), IsPlaying->GetConditionPin());
	OvLink(IsPlaying->GetThenPin(), OvExec(SetActive));

	OvLink(OvPin(Loop, TEXT("Completed"), EGPD_Output), OvExec(DuringEntrance));
	OvLink(OvPin(ActiveGet, ActiveVar, EGPD_Output), DuringEntrance->GetConditionPin());
	OvLink(DuringEntrance->GetThenPin(), OvExec(GetChars));
	OvLink(OvThen(GetChars), OvPin(CharLoop, TEXT("Exec"), EGPD_Input));
	OvLink(OvPin(GetChars, TEXT("OutActors"), EGPD_Output), OvPin(CharLoop, TEXT("Array"), EGPD_Input));
	UEdGraphPin* Character = OvPin(CharLoop, TEXT("Array Element"), EGPD_Output);
	OvLink(OvLoopBody(CharLoop), OvExec(NewBranch));
	OvLink(Character, OvTarget(CharLoc));
	OvLink(OvPin(DefaultGet, DefaultEntranceVar, EGPD_Output), OvPin(BreakDefault, TEXT("InTransform"), EGPD_Input));
	OvLink(OvOut(CharLoc), OvPin(DistDefault, TEXT("V1"), EGPD_Input));
	OvLink(OvPin(BreakDefault, TEXT("Location"), EGPD_Output), OvPin(DistDefault, TEXT("V2"), EGPD_Input));
	OvLink(OvOut(DistDefault), OvPin(NearDefault, TEXT("A"), EGPD_Input));
	OvLink(OvPin(CharsGet1, TrackedCharsVar, EGPD_Output), OvPin(FindIdx, TEXT("TargetArray"), EGPD_Input));
	OvLink(Character, OvPin(FindIdx, TEXT("ItemToFind"), EGPD_Input));
	OvLink(OvOut(FindIdx), OvPin(IsNew, TEXT("A"), EGPD_Input));
	OvLink(OvOut(IsNew), NewBranch->GetConditionPin());
	OvLink(NewBranch->GetThenPin(), OvExec(AddChar));
	OvLink(OvPin(CharsGet2, TrackedCharsVar, EGPD_Output), OvPin(AddChar, TEXT("TargetArray"), EGPD_Input));
	OvLink(Character, OvPin(AddChar, TEXT("NewItem"), EGPD_Input));
	OvLink(OvThen(AddChar), OvExec(AddLoc));
	OvLink(OvPin(LocsGet1, TrackedLocsVar, EGPD_Output), OvPin(AddLoc, TEXT("TargetArray"), EGPD_Input));
	OvLink(OvOut(CharLoc), OvPin(AddLoc, TEXT("NewItem"), EGPD_Input));
	OvLink(OvThen(AddLoc), OvExec(NewNear));
	OvLink(OvOut(NearDefault), NewNear->GetConditionPin());
	OvLink(NewNear->GetThenPin(), OvExec(CharPlace));
	OvLink(NewNear->GetElsePin(), OvExec(Update));
	OvLink(NewBranch->GetElsePin(), OvExec(AtDefault));
	OvLink(OvPin(LocsGet2, TrackedLocsVar, EGPD_Output), Prev->GetTargetArrayPin());
	OvLink(OvOut(FindIdx), Prev->GetIndexPin());
	OvLink(OvOut(CharLoc), OvPin(DistPrev, TEXT("V1"), EGPD_Input));
	OvLink(Prev->GetResultPin(), OvPin(DistPrev, TEXT("V2"), EGPD_Input));
	OvLink(OvOut(DistPrev), OvPin(Jumped, TEXT("A"), EGPD_Input));
	OvLink(OvOut(NearDefault), OvPin(Both, TEXT("A"), EGPD_Input));
	OvLink(OvOut(Jumped), OvPin(Both, TEXT("B"), EGPD_Input));
	OvLink(OvOut(Both), AtDefault->GetConditionPin());
	OvLink(AtDefault->GetThenPin(), OvExec(CharPlace));
	OvLink(AtDefault->GetElsePin(), OvExec(Update));
	OvLink(OvThen(CharPlace), OvExec(Update));
	OvLink(OvPin(LocsGet3, TrackedLocsVar, EGPD_Output), OvPin(Update, TEXT("TargetArray"), EGPD_Input));
	OvLink(OvOut(FindIdx), OvPin(Update, TEXT("Index"), EGPD_Input));
	OvLink(OvOut(CharLoc), OvPin(Update, TEXT("Item"), EGPD_Input));
	OvLink(Character, OvTarget(CharTransform));
	OvLink(OvOut(CharTransform), OvPin(CharCompose, TEXT("A"), EGPD_Input));
	OvLink(Origin3, OvPin(CharCompose, TEXT("B"), EGPD_Input));
	OvLink(OvOut(CharCompose), OvPin(CharPlace, TEXT("NewTransform"), EGPD_Input));
	OvLink(Character, OvTarget(CharPlace));
	OvLink(OvPin(CharLoop, TEXT("Completed"), EGPD_Output), OvExec(FindManager));
	OvLink(DuringEntrance->GetElsePin(), OvExec(ClearChars));
	OvLink(OvPin(CharsGet3, TrackedCharsVar, EGPD_Output), OvPin(ClearChars, TEXT("TargetArray"), EGPD_Input));
	OvLink(OvThen(ClearChars), OvExec(ClearLocs));
	OvLink(OvPin(LocsGet4, TrackedLocsVar, EGPD_Output), OvPin(ClearLocs, TEXT("TargetArray"), EGPD_Input));
	OvLink(OvThen(ClearLocs), OvExec(FindManager));

	OvLink(OvThen(FindManager), OvExec(HasManager));
	OvLink(OvOut(FindManager), OvPin(Valid, TEXT("Object"), EGPD_Input));
	OvLink(OvOut(Valid), HasManager->GetConditionPin());
	OvLink(HasManager->GetThenPin(), OvExec(Moved));
	OvLink(OvOut(FindManager), OvTarget(Observed));
	OvLink(OvOut(Observed), OvPin(Same, TEXT("A"), EGPD_Input));
	OvLink(OvPin(Last, LastManagerVar, EGPD_Output), OvPin(Same, TEXT("B"), EGPD_Input));
	OvLink(OvOut(Same), Moved->GetConditionPin());
	OvLink(Moved->GetElsePin(), OvExec(SetMatch));
	OvLink(OvOut(Observed), OvPin(BreakObserved, TEXT("InTransform"), EGPD_Input));
	OvLink(OvPin(DefaultGet2, DefaultEntranceVar, EGPD_Output), OvPin(BreakDefault2, TEXT("InTransform"), EGPD_Input));
	OvLink(OvPin(BreakObserved, TEXT("Location"), EGPD_Output), OvPin(RootDist, TEXT("V1"), EGPD_Input));
	OvLink(OvPin(BreakDefault2, TEXT("Location"), EGPD_Output), OvPin(RootDist, TEXT("V2"), EGPD_Input));
	OvLink(OvOut(RootDist), OvPin(NearRoot, TEXT("A"), EGPD_Input));
	OvLink(OvOut(NearRoot), OvPin(SetMatch, RootMatchesVar, EGPD_Input));
	OvLink(OvOut(NearRoot), OvPin(MoveAnd, TEXT("A"), EGPD_Input));
	OvLink(OvPin(MoveFlag, MoveManagerVar, EGPD_Output), OvPin(MoveAnd, TEXT("B"), EGPD_Input));
	OvLink(OvThen(SetMatch), OvExec(ShouldMove));
	OvLink(OvOut(MoveAnd), ShouldMove->GetConditionPin());
	OvLink(ShouldMove->GetThenPin(), OvExec(Place));
	OvLink(ShouldMove->GetElsePin(), OvExec(Remember));
	OvLink(OvOut(FindManager), OvTarget(Place));
	OvLink(OvOut(Observed), OvPin(Compose, TEXT("A"), EGPD_Input));
	OvLink(Origin2, OvPin(Compose, TEXT("B"), EGPD_Input));
	OvLink(OvOut(Compose), OvPin(Place, TEXT("NewTransform"), EGPD_Input));
	OvLink(OvThen(Place), OvExec(Remember));
	OvLink(OvOut(FindManager), OvTarget(After));
	OvLink(OvOut(After), OvPin(Remember, LastManagerVar, EGPD_Input));

	OvFinishBlueprint(BP);
	return BP;
}

bool UAEWArenaTools::UpdateEntranceSwaps(bool bReport)
{
	UWorld* World = OvEditorWorld();
	if (!World)
	{
		return false;
	}
	const FString MapName = FPackageName::GetShortName(World->GetOutermost()->GetName());
	int32 ArenaId = INDEX_NONE;
	TArray<FAEWArenaListEntry> Arenas;
	GetArenaList(Arenas);
	for (const FAEWArenaListEntry& E : Arenas)
	{
		FString Short = E.LevelName;
		Short.Split(TEXT("."), &Short, nullptr);
		Short = FPackageName::GetShortName(Short);
		if (Short.Equals(MapName, ESearchCase::IgnoreCase) && E.Id.IsNumeric())
		{
			ArenaId = FCString::Atoi(*E.Id);
		}
	}
	if (ArenaId == INDEX_NONE)
	{
		if (bReport)
		{
			OvReport(FString::Printf(TEXT("%s isn't in the arena list. Run Sync Arena List first."), *MapName));
		}
		return false;
	}

	TMap<int32, int32> Pairs;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		UClass* C = It->GetClass();
		if (C->GetName() != TEXT("BP_EntranceOverride_C"))
		{
			continue;
		}
		FBoolProperty* Force = FindFProperty<FBoolProperty>(C, ForceStageVar);
		FArrayProperty* From = FindFProperty<FArrayProperty>(C, SwapFromVar);
		FArrayProperty* To = FindFProperty<FArrayProperty>(C, SwapToVar);
		if (!Force || !From || !To || !Force->GetPropertyValue_InContainer(*It))
		{
			continue;
		}
		FScriptArrayHelper FromHelper(From, From->ContainerPtrToValuePtr<void>(*It));
		FScriptArrayHelper ToHelper(To, To->ContainerPtrToValuePtr<void>(*It));
		for (int32 i = 0; i < FMath::Min(FromHelper.Num(), ToHelper.Num()); ++i)
		{
			Pairs.Add(*reinterpret_cast<int32*>(FromHelper.GetRawPtr(i)), *reinterpret_cast<int32*>(ToHelper.GetRawPtr(i)));
		}
	}

	const FString TablePath = TEXT("/Game/GameData/Assets/DataTable/Cutscene/DT_ConvertEntranceCutsceneList");
	const FString StructPath = TEXT("/Game/GameData/Assets/Struct/CutScene/FConvertEntranceCutsceneParam.FConvertEntranceCutsceneParam");
	UDataTable* Table = LoadObject<UDataTable>(nullptr, *(TablePath + TEXT(".DT_ConvertEntranceCutsceneList")), nullptr, LOAD_NoWarn | LOAD_Quiet);
	UScriptStruct* RowStruct = Table ? const_cast<UScriptStruct*>(Table->GetRowStruct()) : LoadObject<UScriptStruct>(nullptr, *StructPath);
	FMapProperty* MapProp = nullptr;
	for (TFieldIterator<FMapProperty> It(RowStruct); RowStruct && It; ++It)
	{
		MapProp = *It;
		break;
	}
	UEnum* ArenaEnum = MapProp ? (CastField<FByteProperty>(MapProp->KeyProp) ? CastField<FByteProperty>(MapProp->KeyProp)->Enum : (CastField<FEnumProperty>(MapProp->KeyProp) ? CastField<FEnumProperty>(MapProp->KeyProp)->GetEnum() : nullptr)) : nullptr;
	if (!RowStruct || !MapProp || !CastField<FIntProperty>(MapProp->ValueProp))
	{
		if (bReport)
		{
			OvReport(TEXT("FConvertEntranceCutsceneParam not found; entrance swaps can't be written."));
		}
		return false;
	}

	bool bCreated = false;
	if (!Table)
	{
		if (Pairs.Num() == 0)
		{
			if (bReport)
			{
				OvReport(TEXT("No entrance swaps on and nothing to clean up."));
			}
			return true;
		}
		UPackage* Package = CreatePackage(*TablePath);
		Table = NewObject<UDataTable>(Package, TEXT("DT_ConvertEntranceCutsceneList"), RF_Public | RF_Standalone | RF_Transactional);
		Table->RowStruct = RowStruct;
		FAssetRegistryModule::AssetCreated(Table);
		bCreated = true;
		uint8* Row = FDataTableEditorUtils::AddRow(Table, TEXT("40"));
		if (Row)
		{
			FScriptMapHelper Map(MapProp, MapProp->ContainerPtrToValuePtr<void>(Row));
			for (int32 Original = 13; Original <= 16; ++Original)
			{
				int64 Key = ArenaEnum ? ArenaEnum->GetValueByNameString(FString::Printf(TEXT("NewEnumerator%d"), Original)) : Original;
				if (Key == INDEX_NONE)
				{
					Key = Original;
				}
				uint8 KeyByte = static_cast<uint8>(Key);
				int32 Value = 125;
				Map.AddPair(&KeyByte, &Value);
			}
		}
	}

	const uint8 MyKey = static_cast<uint8>(ArenaId);
	if (!bCreated)
	{
		TMap<int32, int32> Current;
		for (const TPair<FName, uint8*>& R : Table->GetRowMap())
		{
			FScriptMapHelper Map(MapProp, MapProp->ContainerPtrToValuePtr<void>(R.Value));
			for (int32 i = 0; i < Map.GetMaxIndex(); ++i)
			{
				if (Map.IsValidIndex(i) && *Map.GetKeyPtr(i) == MyKey)
				{
					Current.Add(FCString::Atoi(*R.Key.ToString()), *reinterpret_cast<const int32*>(Map.GetValuePtr(i)));
				}
			}
		}
		bool bSame = Current.Num() == Pairs.Num();
		for (const TPair<int32, int32>& P : Pairs)
		{
			const int32* Found = Current.Find(P.Key);
			bSame = bSame && Found && *Found == P.Value;
		}
		if (bSame)
		{
			if (bReport)
			{
				OvReport(FString::Printf(TEXT("Entrance swaps for arena %d are up to date."), ArenaId));
			}
			return true;
		}
	}

	Table->Modify();
	TArray<FName> Empty;
	for (const TPair<FName, uint8*>& R : Table->GetRowMap())
	{
		FScriptMapHelper Map(MapProp, MapProp->ContainerPtrToValuePtr<void>(R.Value));
		for (int32 i = Map.GetMaxIndex() - 1; i >= 0; --i)
		{
			if (Map.IsValidIndex(i) && *Map.GetKeyPtr(i) == MyKey)
			{
				Map.RemoveAt(i);
			}
		}
		Map.Rehash();
		if (Map.Num() == 0)
		{
			Empty.Add(R.Key);
		}
	}
	for (const FName& Name : Empty)
	{
		FDataTableEditorUtils::RemoveRow(Table, Name);
	}
	FString Summary;
	for (const TPair<int32, int32>& P : Pairs)
	{
		const FName RowName(*FString::FromInt(P.Key));
		uint8* Row = Table->FindRowUnchecked(RowName);
		if (!Row)
		{
			Row = FDataTableEditorUtils::AddRow(Table, RowName);
		}
		if (!Row)
		{
			continue;
		}
		FScriptMapHelper Map(MapProp, MapProp->ContainerPtrToValuePtr<void>(Row));
		uint8 KeyByte = MyKey;
		int32 Value = P.Value;
		Map.AddPair(&KeyByte, &Value);
		Summary += FString::Printf(TEXT("\n  %d -> %d"), P.Key, P.Value);
	}
	FDataTableEditorUtils::BroadcastPostChange(Table, FDataTableEditorUtils::EDataTableChangeInfo::RowList);
	Table->MarkPackageDirty();
	UEditorLoadingAndSavingUtils::SavePackages({ Table->GetOutermost() }, false);
	if (UAEWModdingSettings* ModSettings = GetMutableDefault<UAEWModdingSettings>())
	{
		const FSoftObjectPath TableRef(Table);
		if (!ModSettings->AlwaysLabelTables.Contains(TableRef))
		{
			ModSettings->AlwaysLabelTables.Add(TableRef);
			ModSettings->UpdateDefaultConfigFile();
		}
	}

	if (!bReport)
	{
		UE_LOG(LogAEWArenaOverrides, Log, TEXT("DT_ConvertEntranceCutsceneList: updated %d entrance swap(s) for arena %d"), Pairs.Num(), ArenaId);
	}
	else
	{
		OvReport(Pairs.Num() == 0
			? FString::Printf(TEXT("Removed entrance swaps for arena %d."), ArenaId)
			: FString::Printf(TEXT("DT_ConvertEntranceCutsceneList%s: arena %d now swaps:%s\n\nIt's added to Always Label Tables so it's packaged with your mod."),
				bCreated ? TEXT(" (created)") : TEXT(""), ArenaId, *Summary));
	}
	return true;
}

bool UAEWArenaTools::AddEntranceOverride()
{
	UWorld* World = OvEditorWorld();
	if (!World)
	{
		return false;
	}
	UBlueprint* BP = EnsureEntranceOverrideBlueprint(OvArenaFolderOf(World), true);
	UClass* OverrideClass = BP ? BP->GeneratedClass.Get() : nullptr;
	FStructProperty* DefaultProp = OverrideClass ? FindFProperty<FStructProperty>(OverrideClass, DefaultEntranceVar) : nullptr;
	if (!DefaultProp)
	{
		OvReport(TEXT("Could not create BP_EntranceOverride - see the Output Log."));
		return false;
	}

	struct FOvEntranceRoot
	{
		const TCHAR* Label;
		FVector Location;
		const TCHAR* What;
	};
	const FOvEntranceRoot Roots[] =
	{
		{ TEXT("EntranceOrigin"), DefaultEntranceLocation, TEXT("the stage (nearly every entrance)") },
		{ TEXT("EntranceOrigin_Crowd"), FVector(-613.f, 688.f, -100.f), TEXT("the crowd (Jon Moxley)") },
		{ TEXT("EntranceOrigin_Ring"), FVector(-312.f, 0.f, 3.f), TEXT("the ring (Adam Cole)") },
	};

	FStrProperty* ListProp = FindFProperty<FStrProperty>(OverrideClass, SequenceListVar);
	FBoolProperty* ExclProp = FindFProperty<FBoolProperty>(OverrideClass, ExclusionVar);
	FBoolProperty* ForceProp = FindFProperty<FBoolProperty>(OverrideClass, ForceStageVar);
	auto Configure = [&](AActor* Origin)
	{
		if (!ListProp || !ExclProp)
		{
			return;
		}
		const FVector L = DefaultProp->ContainerPtrToValuePtr<FTransform>(Origin)->GetLocation();
		Origin->Modify();
		if (FVector::Dist(L, Roots[1].Location) < 1.f)
		{
			ListProp->SetPropertyValue_InContainer(Origin, CrowdSequences);
			ExclProp->SetPropertyValue_InContainer(Origin, false);
			if (ForceProp) ForceProp->SetPropertyValue_InContainer(Origin, false);
		}
		else if (FVector::Dist(L, Roots[2].Location) < 1.f)
		{
			ListProp->SetPropertyValue_InContainer(Origin, RingSequences);
			ExclProp->SetPropertyValue_InContainer(Origin, false);
			if (ForceProp) ForceProp->SetPropertyValue_InContainer(Origin, false);
		}
		else
		{
			ListProp->SetPropertyValue_InContainer(Origin, FString::Printf(TEXT("%s,%s"), CrowdSequences, RingSequences));
			ExclProp->SetPropertyValue_InContainer(Origin, true);
			if (ForceProp) ForceProp->SetPropertyValue_InContainer(Origin, true);
		}
	};

	TArray<FVector> Existing;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->GetClass() == OverrideClass)
		{
			Existing.Add(DefaultProp->ContainerPtrToValuePtr<FTransform>(*It)->GetLocation());
			Configure(*It);
		}
	}

	const FScopedTransaction Transaction(LOCTEXT("AddEntranceOverride", "Add Entrance Override"));
	World->PersistentLevel->Modify();
	GEditor->SelectNone(false, true);
	FString Added;
	for (const FOvEntranceRoot& Root : Roots)
	{
		bool bHave = false;
		for (const FVector& L : Existing)
		{
			bHave |= FVector::Dist(L, Root.Location) < 1.f;
		}
		if (bHave)
		{
			continue;
		}
		FActorSpawnParameters Params;
		Params.OverrideLevel = World->PersistentLevel;
		const FTransform Default(FRotator::ZeroRotator, Root.Location);
		AActor* Origin = World->SpawnActor<AActor>(OverrideClass, Default, Params);
		if (!Origin)
		{
			continue;
		}
		Origin->SetActorLabel(Root.Label);
		Origin->SetFolderPath(TEXT("ArenaOverrides/Entrances"));
		*DefaultProp->ContainerPtrToValuePtr<FTransform>(Origin) = Default;
		Configure(Origin);
		GEditor->SelectActor(Origin, true, true);
		Added += FString::Printf(TEXT("\n  %s - %s"), Root.Label, Root.What);
	}
	World->PersistentLevel->MarkPackageDirty();
	UpdateEntranceSwaps(false);

	OvReport((Added.IsEmpty() ? FString(TEXT("Every entrance spot already has an EntranceOrigin.")) : TEXT("Added:") + Added)
		+ TEXT("\n\nMove the origins to change, then save the map. ")
		TEXT("Unmoved origins change nothing.\n\n")
		TEXT("Turn off the reverse-entrance flag for this arena; this replaces it."));
	return true;
}

namespace
{
	const TCHAR* WeaponMarkerName = TEXT("BP_WeaponPointMarker");
	const FName TargetPathVar(TEXT("TargetClassPath"));
	const FName PointNameVar(TEXT("PointName"));
	const FName PointDefaultVar(TEXT("DefaultTransform"));
	const FName LastTargetVar(TEXT("LastTarget"));
	const FName TargetClassVar(TEXT("TargetClass"));
	const FName WeaponClassVar(TEXT("WeaponClass"));
	const FName OldPointVar(TEXT("OldPoint"));
	const FName NewPointVar(TEXT("NewPoint"));
	const TCHAR* WeaponClassPath = TEXT("/Game/GameData/Assets/Weapon/BP_Weapon.BP_Weapon_C");

	struct FOvWeaponPoint
	{
		const TCHAR* ClassName;
		const TCHAR* Point;
		FVector Location;
		FRotator Rotation;
	};

	const FOvWeaponPoint WeaponPoints[] =
	{
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("Init_10"), FVector(-1.0f, 502.0f, -100.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("Init_11"), FVector(-510.0f, 0.0f, -100.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("Init_12"), FVector(510.0f, 1.0f, -100.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("Init_2"), FVector(0.0f, 0.0f, 0.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("Init_3"), FVector(-199.0f, -210.0f, 0.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("Init_4"), FVector(197.0f, 182.0f, 0.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("Init_5"), FVector(-510.0f, -500.0f, -100.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("Init_6"), FVector(-510.0f, 500.0f, -100.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("Init_7"), FVector(510.0f, 500.0f, -100.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("Init_8"), FVector(510.0f, -500.0f, -100.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("Init_9"), FVector(-14.0f, -500.0f, -100.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("Outside_10"), FVector(540.0f, 280.0f, -100.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("Outside_12"), FVector(280.0f, 550.0f, -100.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("Outside_14"), FVector(-170.0f, 530.0f, -100.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("Outside_16"), FVector(-520.0f, 310.0f, -100.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("Outside_17"), FVector(-510.0f, 130.0f, -100.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("Outside_18"), FVector(-520.0f, -140.0f, -100.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("Outside_19"), FVector(-510.0f, -330.0f, -100.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("Outside_21"), FVector(1000.0f, 0.0f, -100.0f), FRotator(10.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("Outside_23"), FVector(1500.0f, 0.0f, -18.0609f), FRotator(10.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("Outside_24"), FVector(1750.0f, 0.0f, 32.2971f), FRotator(10.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("Outside_26"), FVector(2250.0f, 0.0f, 115.648f), FRotator(10.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("Outside_27"), FVector(2500.0f, 0.0f, 115.648f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("Outside_28"), FVector(2750.0f, 0.0f, 115.648f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("Outside_29"), FVector(2650.0f, -200.0f, 115.648f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("Outside_2"), FVector(320.0f, -500.0f, -100.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("Outside_30"), FVector(2680.0f, 160.0f, 115.648f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("Outside_31"), FVector(2490.0f, 330.0f, 115.648f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("Outside_32"), FVector(2480.0f, -330.0f, 115.648f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("Outside_34"), FVector(-510.0f, 0.0f, -100.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("Outside_35"), FVector(0.0f, -500.0f, -100.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("Outside_36"), FVector(0.0f, 500.0f, -100.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("Outside_37"), FVector(530.0f, 0.0f, -100.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("Outside_38"), FVector(2140.0f, -370.0f, -100.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("Outside_39"), FVector(2140.0f, 370.0f, -100.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("Outside_3"), FVector(140.0f, -500.0f, -100.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("Outside_40"), FVector(2550.0f, 220.0f, 100.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("Outside_41"), FVector(2540.0f, -220.0f, 100.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("Outside_4"), FVector(-170.0f, -500.0f, -100.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("Outside_5"), FVector(-340.0f, -500.0f, -100.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("Outside_7"), FVector(550.0f, -330.0f, -100.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("Outside_8"), FVector(540.0f, -140.0f, -100.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("Outside_9"), FVector(560.0f, 130.0f, -100.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("Ring_12"), FVector(0.0f, 250.0f, 0.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("Ring_13"), FVector(-250.0f, 250.0f, 0.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("Ring_14"), FVector(0.0f, 0.0f, 0.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("Ring_1"), FVector(250.0f, -250.0f, 0.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("Ring_2"), FVector(0.0f, -250.0f, 0.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("Ring_6"), FVector(250.0f, 0.0f, 0.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("Ring_8"), FVector(-250.0f, 0.0f, 0.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("ThrowOther_00"), FVector(1200.0f, -500.0f, 0.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("ThrowOther_01"), FVector(1200.0f, 500.0f, 0.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("ThrowOther_02"), FVector(1500.0f, -500.0f, 0.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("ThrowOther_03"), FVector(1500.0f, 500.0f, 0.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("ThrowOther_04"), FVector(1800.0f, -500.0f, 0.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("ThrowOther_05"), FVector(1800.0f, 500.0f, 0.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("ThrowRing_00"), FVector(-850.0f, -600.0f, 0.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("ThrowRing_01"), FVector(-850.0f, -400.0f, 0.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("ThrowRing_02"), FVector(-850.0f, -200.0f, 0.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("ThrowRing_03"), FVector(-850.0f, 0.0f, 0.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("ThrowRing_04"), FVector(-850.0f, 200.0f, 0.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("ThrowRing_05"), FVector(-850.0f, 400.0f, 0.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("ThrowRing_06"), FVector(-850.0f, 600.0f, 0.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("ThrowRing_07"), FVector(-600.0f, -900.0f, 0.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("ThrowRing_08"), FVector(-600.0f, 900.0f, 0.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("ThrowRing_09"), FVector(-300.0f, -900.0f, 0.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("ThrowRing_10"), FVector(-300.0f, 900.0f, 0.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("ThrowRing_11"), FVector(0.0f, -900.0f, 0.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("ThrowRing_12"), FVector(0.0f, 900.0f, 0.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("ThrowRing_13"), FVector(300.0f, -900.0f, 0.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("ThrowRing_14"), FVector(300.0f, 900.0f, 0.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("ThrowRing_15"), FVector(600.0f, -900.0f, 0.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("ThrowRing_16"), FVector(600.0f, 900.0f, 0.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("ThrowRing_17"), FVector(900.0f, -500.0f, 0.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_LightsOutWeaponGenerator_C"), TEXT("ThrowRing_18"), FVector(900.0f, 500.0f, 0.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_WeaponPutSettingPoint_C"), TEXT("2"), FVector(-200.0f, -200.0f, 0.0f), FRotator(0.0f, 45.0f, 0.0f) },
		{ TEXT("BP_WeaponPutSettingPoint_C"), TEXT("3"), FVector(200.0f, -200.0f, 0.0f), FRotator(0.0f, 135.0f, 0.0f) },
		{ TEXT("BP_WeaponPutSettingPoint_C"), TEXT("4"), FVector(200.0f, 200.0f, 0.0f), FRotator(0.0f, -135.0f, 0.0f) },
		{ TEXT("BP_WeaponPutSettingPoint_C"), TEXT("5"), FVector(-200.0f, 200.0f, 0.0f), FRotator(0.0f, -45.0f, 0.0f) },
		{ TEXT("BP_WeaponPutSettingPoint_C"), TEXT("Ladder_Rope1"), FVector(0.0f, -310.0f, 0.0f), FRotator(0.0f, 90.0f, 0.0f) },
		{ TEXT("BP_WeaponPutSettingPoint_C"), TEXT("Ladder_Rope2"), FVector(310.0f, 0.0f, 0.0f), FRotator(0.0f, -180.0f, 0.0f) },
		{ TEXT("BP_WeaponPutSettingPoint_C"), TEXT("Ladder_Rope3"), FVector(0.0f, 310.0f, 0.0f), FRotator(0.0f, -90.0f, 0.0f) },
		{ TEXT("BP_WeaponPutSettingPoint_C"), TEXT("Ladder_Rope4"), FVector(-310.0f, 0.0f, 0.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_WeaponPutSettingPoint_C"), TEXT("\u30e9\u30c0\u30fc\u30ea\u30f3\u30b0\u5185\u4e2d\u592e"), FVector(0.0f, 0.0f, 0.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_WeaponPutSettingPoint_C"), TEXT("\u753b\u3073\u3087\u3046_10"), FVector(0.0f, 550.0f, -100.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_WeaponPutSettingPoint_C"), TEXT("\u753b\u3073\u3087\u3046_11"), FVector(0.0f, -550.0f, -100.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_WeaponPutSettingPoint_C"), TEXT("\u753b\u3073\u3087\u3046_12"), FVector(150.0f, -550.0f, -100.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_WeaponPutSettingPoint_C"), TEXT("\u753b\u3073\u3087\u3046_13"), FVector(300.0f, -550.0f, -100.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_WeaponPutSettingPoint_C"), TEXT("\u753b\u3073\u3087\u3046_14"), FVector(-150.0f, -550.0f, -100.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_WeaponPutSettingPoint_C"), TEXT("\u753b\u3073\u3087\u3046_15"), FVector(-300.0f, -550.0f, -100.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_WeaponPutSettingPoint_C"), TEXT("\u753b\u3073\u3087\u3046_16"), FVector(150.0f, 550.0f, -100.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_WeaponPutSettingPoint_C"), TEXT("\u753b\u3073\u3087\u3046_17"), FVector(300.0f, 550.0f, -100.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_WeaponPutSettingPoint_C"), TEXT("\u753b\u3073\u3087\u3046_18"), FVector(-150.0f, 550.0f, -100.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_WeaponPutSettingPoint_C"), TEXT("\u753b\u3073\u3087\u3046_19"), FVector(-300.0f, 550.0f, -100.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_WeaponPutSettingPoint_C"), TEXT("\u753b\u3073\u3087\u3046_1"), FVector(0.0f, 0.0f, 0.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_WeaponPutSettingPoint_C"), TEXT("\u753b\u3073\u3087\u3046_20"), FVector(-450.0f, 550.0f, -100.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_WeaponPutSettingPoint_C"), TEXT("\u753b\u3073\u3087\u3046_21"), FVector(450.0f, 550.0f, -100.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_WeaponPutSettingPoint_C"), TEXT("\u753b\u3073\u3087\u3046_22"), FVector(450.0f, -550.0f, -100.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_WeaponPutSettingPoint_C"), TEXT("\u753b\u3073\u3087\u3046_23"), FVector(-450.0f, -550.0f, -100.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_WeaponPutSettingPoint_C"), TEXT("\u753b\u3073\u3087\u3046_2"), FVector(0.0f, 150.0f, 0.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_WeaponPutSettingPoint_C"), TEXT("\u753b\u3073\u3087\u3046_3"), FVector(0.0f, -150.0f, 0.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_WeaponPutSettingPoint_C"), TEXT("\u753b\u3073\u3087\u3046_4"), FVector(150.0f, 0.0f, 0.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_WeaponPutSettingPoint_C"), TEXT("\u753b\u3073\u3087\u3046_5"), FVector(-150.0f, 0.0f, 0.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_WeaponPutSettingPoint_C"), TEXT("\u753b\u3073\u3087\u3046_6"), FVector(-150.0f, 150.0f, 0.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_WeaponPutSettingPoint_C"), TEXT("\u753b\u3073\u3087\u3046_7"), FVector(-150.0f, -150.0f, 0.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_WeaponPutSettingPoint_C"), TEXT("\u753b\u3073\u3087\u3046_8"), FVector(150.0f, -150.0f, 0.0f), FRotator(0.0f, 0.0f, 0.0f) },
		{ TEXT("BP_WeaponPutSettingPoint_C"), TEXT("\u753b\u3073\u3087\u3046_9"), FVector(150.0f, 150.0f, 0.0f), FRotator(0.0f, 0.0f, 0.0f) },
	};

	FString OvWeaponOwnerPath(const FString& ClassName)
	{
		const FString Asset = ClassName.LeftChop(2);
		return FString::Printf(TEXT("/Game/GameData/Assets/MatchRule/%s.%s"), *Asset, *ClassName);
	}

	void OvAddVar(UBlueprint* BP, const FName Name, const FEdGraphPinType& Type, bool bEditable, const FString& Tooltip)
	{
		if (FBlueprintEditorUtils::FindNewVariableIndex(BP, Name) != INDEX_NONE)
		{
			return;
		}
		FBlueprintEditorUtils::AddMemberVariable(BP, Name, Type);
		if (bEditable)
		{
			FBlueprintEditorUtils::SetBlueprintOnlyEditableFlag(BP, Name, false);
			FBlueprintEditorUtils::SetBlueprintVariableMetaData(BP, Name, nullptr, TEXT("tooltip"), Tooltip);
			FBlueprintEditorUtils::SetBlueprintVariableCategory(BP, Name, nullptr, FText::FromString(TEXT("Arena Override")));
		}
	}

	UEdGraphPin* OvLoadCachedClass(UEdGraph* G, UEdGraphPin*& InOutExec, UEdGraphPin*& OutFail, const FName CacheVar, UEdGraphPin* PathPin, const TCHAR* PathLiteral, int32 X, int32 Y)
	{
		UK2Node_VariableGet* Cached = OvGetSelfVar(G, CacheVar, X, Y + 150);
		UK2Node_CallFunction* Valid = OvCall(G, UKismetSystemLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, IsValidClass), X, Y + 250);
		UK2Node_IfThenElse* Have = OvBranch(G, X + 200, Y);
		UK2Node_CallFunction* Make = OvCall(G, UKismetSystemLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, MakeSoftClassPath), X + 200, Y + 250);
		UK2Node_CallFunction* Conv = OvCall(G, UKismetSystemLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, Conv_SoftClassPathToSoftClassRef), X + 450, Y + 250);
		UK2Node_CallFunction* Load = OvCall(G, UKismetSystemLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, LoadClassAsset_Blocking), X + 450, Y);
		UK2Node_VariableSet* Store = OvSetSelfVar(G, CacheVar, X + 750, Y);
		UK2Node_CallFunction* Sink = OvCall(G, UKismetSystemLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, IsValidClass), X + 1000, Y + 250);
		UK2Node_IfThenElse* Join = OvBranch(G, X + 1000, Y);
		if (!Valid || !Make || !Conv || !Load || !Sink)
		{
			return nullptr;
		}
		OvLink(InOutExec, OvExec(Have));
		OvLink(OvPin(Cached, CacheVar, EGPD_Output), OvPin(Valid, TEXT("Class"), EGPD_Input));
		OvLink(OvOut(Valid), Have->GetConditionPin());
		OvLink(Have->GetElsePin(), OvExec(Load));
		if (PathPin)
		{
			OvLink(PathPin, OvPin(Make, TEXT("PathString"), EGPD_Input));
		}
		else
		{
			OvSetValue(OvPin(Make, TEXT("PathString"), EGPD_Input), PathLiteral);
		}
		OvLink(OvOut(Make), OvPin(Conv, TEXT("SoftClassPath"), EGPD_Input));
		OvLink(OvOut(Conv), OvPin(Load, TEXT("AssetClass"), EGPD_Input));
		UK2Node_ClassDynamicCast* AsActor = NewObject<UK2Node_ClassDynamicCast>(G);
		AsActor->TargetType = AActor::StaticClass();
		OvPlace(G, AsActor, X + 600, Y);
		OvLink(OvThen(Load), OvExec(AsActor));
		OvLink(OvOut(Load), AsActor->GetCastSourcePin());
		OvLink(AsActor->GetValidCastPin(), OvExec(Store));
		OvLink(AsActor->GetCastResultPin(), OvPin(Store, CacheVar, EGPD_Input));
		OvLink(OvThen(Store), OvExec(Join));
		OvLink(AsActor->GetInvalidCastPin(), OvExec(Join));
		OvLink(Have->GetThenPin(), OvExec(Join));
		UK2Node_VariableGet* Again = OvGetSelfVar(G, CacheVar, X + 1000, Y + 400);
		OvLink(OvPin(Again, CacheVar, EGPD_Output), OvPin(Sink, TEXT("Class"), EGPD_Input));
		OvLink(OvOut(Sink), Join->GetConditionPin());
		InOutExec = Join->GetThenPin();
		OutFail = Join->GetElsePin();
		UK2Node_VariableGet* Result = OvGetSelfVar(G, CacheVar, X + 1250, Y + 150);
		return OvPin(Result, CacheVar, EGPD_Output);
	}
}

UBlueprint* UAEWArenaTools::EnsureWeaponMarkerBlueprint(const FString& ArenaFolder, bool bRebuild)
{
	bool bNew = false;
	UBlueprint* BP = OvNewActorBlueprint(ArenaFolder, WeaponMarkerName, bRebuild, bNew);
	if (!BP || (!bNew && !bRebuild))
	{
		return BP;
	}
	if (bNew)
	{
		USCS_Node* SphereNode = OvAddComponent(BP, USphereComponent::StaticClass(), TEXT("Point"), nullptr);
		if (USphereComponent* Sphere = Cast<USphereComponent>(SphereNode->ComponentTemplate))
		{
			Sphere->InitSphereRadius(12.f);
			Sphere->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
			Sphere->SetHiddenInGame(true);
			Sphere->ShapeColor = FColor(255, 220, 0);
		}
		USCS_Node* ArrowNode = OvAddComponent(BP, UArrowComponent::StaticClass(), TEXT("Facing"), SphereNode);
		if (UArrowComponent* Arrow = Cast<UArrowComponent>(ArrowNode->ComponentTemplate))
		{
			Arrow->ArrowColor = FColor(255, 220, 0);
			Arrow->ArrowSize = 0.6f;
			Arrow->SetHiddenInGame(true);
		}
	}
	FEdGraphPinType StrType;
	StrType.PinCategory = UEdGraphSchema_K2::PC_String;
	FEdGraphPinType TransformType;
	TransformType.PinCategory = UEdGraphSchema_K2::PC_Struct;
	TransformType.PinSubCategoryObject = TBaseStructure<FTransform>::Get();
	FEdGraphPinType ActorType;
	ActorType.PinCategory = UEdGraphSchema_K2::PC_Object;
	ActorType.PinSubCategoryObject = AActor::StaticClass();
	FEdGraphPinType ClassType;
	ClassType.PinCategory = UEdGraphSchema_K2::PC_Class;
	ClassType.PinSubCategoryObject = AActor::StaticClass();
	OvAddVar(BP, TargetPathVar, StrType, true, TEXT("Class path of the actor that owns the point."));
	OvAddVar(BP, PointNameVar, StrType, true, TEXT("Point (arrow component) name, e.g. Init_10."));
	OvAddVar(BP, PointDefaultVar, TransformType, true, TEXT("Default point position. The point moves by the offset from here to this marker."));
	OvAddVar(BP, LastTargetVar, ActorType, false, FString());
	OvAddVar(BP, TargetClassVar, ClassType, false, FString());
	OvAddVar(BP, WeaponClassVar, ClassType, false, FString());
	OvAddVar(BP, OldPointVar, TransformType, false, FString());
	OvAddVar(BP, NewPointVar, TransformType, false, FString());

	UEdGraph* G = FBlueprintEditorUtils::FindEventGraph(BP);
	UK2Node_Event* Begin = OvBeginPlayNode(G, 0, -300);
	UK2Node_CallFunction* StartTimer = OvCall(G, UKismetSystemLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, K2_SetTimer), 300, -300);
	OvSetValue(OvPin(StartTimer, TEXT("FunctionName"), EGPD_Input), TryApplyEvent.ToString());
	OvSetValue(OvPin(StartTimer, TEXT("Time"), EGPD_Input), TEXT("0.25"));
	OvSetValue(OvPin(StartTimer, TEXT("bLooping"), EGPD_Input), TEXT("true"));
	UK2Node_CustomEvent* Try = NewObject<UK2Node_CustomEvent>(G);
	Try->CustomFunctionName = TryApplyEvent;
	OvPlace(G, Try, 0, 0);
	if (!StartTimer)
	{
		return nullptr;
	}
	OvLink(OvThen(Begin), OvExec(StartTimer));

	UEdGraphPin* Exec = OvThen(Try);
	UK2Node_VariableGet* Path = OvGetSelfVar(G, TargetPathVar, 0, 300);
	UEdGraphPin* TargetFail = nullptr;
	UEdGraphPin* TargetClass = OvLoadCachedClass(G, Exec, TargetFail, TargetClassVar, OvPin(Path, TargetPathVar, EGPD_Output), nullptr, 200, 0);
	UK2Node_CallFunction* Find = OvCall(G, UGameplayStatics::StaticClass(), GET_FUNCTION_NAME_CHECKED(UGameplayStatics, GetActorOfClass), 1500, 0);
	UK2Node_CallFunction* Valid = OvCall(G, UKismetSystemLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, IsValid), 1750, 200);
	UK2Node_VariableGet* Last = OvGetSelfVar(G, LastTargetVar, 1750, 300);
	UK2Node_CallFunction* Differs = OvCall(G, UKismetMathLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, NotEqual_ObjectObject), 1950, 300);
	UK2Node_CallFunction* Both = OvCall(G, UKismetMathLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, BooleanAND), 2150, 250);
	UK2Node_IfThenElse* NewTarget = OvBranch(G, 2350, 0);
	UK2Node_CallFunction* Comps = OvCall(G, AActor::StaticClass(), GET_FUNCTION_NAME_CHECKED(AActor, K2_GetComponentsByClass), 2600, 0);
	OvSetObject(OvPin(Comps, TEXT("ComponentClass"), EGPD_Input), USceneComponent::StaticClass());
	UK2Node_MacroInstance* CompLoop = NewObject<UK2Node_MacroInstance>(G);
	CompLoop->SetMacroGraph(OvForEachMacro());
	OvPlace(G, CompLoop, 2900, 0);
	UK2Node_CallFunction* CompName = OvCall(G, UKismetSystemLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, GetObjectName), 2900, 250);
	UK2Node_VariableGet* Wanted = OvGetSelfVar(G, PointNameVar, 2900, 350);
	UK2Node_CallFunction* Match = OvCall(G, UKismetStringLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetStringLibrary, EqualEqual_StrStr), 3150, 250);
	UK2Node_IfThenElse* IsPoint = OvBranch(G, 3350, 0);
	UK2Node_CallFunction* CompWorld = OvCall(G, USceneComponent::StaticClass(), GET_FUNCTION_NAME_CHECKED(USceneComponent, K2_GetComponentToWorld), 3350, 250);
	UK2Node_VariableSet* SetOld = OvSetSelfVar(G, OldPointVar, 3600, 0);
	UK2Node_VariableGet* OldGet1 = OvGetSelfVar(G, OldPointVar, 3600, 250);
	UK2Node_VariableGet* DefaultGet = OvGetSelfVar(G, PointDefaultVar, 3600, 350);
	UK2Node_CallFunction* InvDefault = OvCall(G, UKismetMathLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, InvertTransform), 3800, 350);
	UK2Node_CallFunction* Mine = OvCall(G, AActor::StaticClass(), GET_FUNCTION_NAME_CHECKED(AActor, GetTransform), 3800, 450);
	UK2Node_CallFunction* Delta = OvCall(G, UKismetMathLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, ComposeTransforms), 4000, 350);
	UK2Node_CallFunction* Moved = OvCall(G, UKismetMathLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, ComposeTransforms), 4200, 250);
	UK2Node_VariableSet* SetNew = OvSetSelfVar(G, NewPointVar, 4400, 0);
	UEdGraphPin* WeaponExec = OvThen(SetNew);
	UEdGraphPin* WeaponFail = nullptr;
	UEdGraphPin* WeaponClass = OvLoadCachedClass(G, WeaponExec, WeaponFail, WeaponClassVar, nullptr, WeaponClassPath, 4650, 0);
	UK2Node_CallFunction* Weapons = OvCall(G, UGameplayStatics::StaticClass(), GET_FUNCTION_NAME_CHECKED(UGameplayStatics, GetAllActorsOfClass), 5950, 0);
	UK2Node_MacroInstance* WeaponLoop = NewObject<UK2Node_MacroInstance>(G);
	WeaponLoop->SetMacroGraph(OvForEachMacro());
	OvPlace(G, WeaponLoop, 6250, 0);
	UK2Node_CallFunction* WeaponLoc = OvCall(G, AActor::StaticClass(), GET_FUNCTION_NAME_CHECKED(AActor, K2_GetActorLocation), 6250, 250);
	UK2Node_VariableGet* OldGet2 = OvGetSelfVar(G, OldPointVar, 6250, 350);
	UK2Node_CallFunction* BreakOld = OvCall(G, UKismetMathLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, BreakTransform), 6450, 350);
	UK2Node_CallFunction* Dist = OvCall(G, UKismetMathLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Vector_Distance), 6700, 250);
	UK2Node_CallFunction* Near = OvCall(G, UKismetMathLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Less_FloatFloat), 6900, 250);
	OvSetValue(OvPin(Near, TEXT("B"), EGPD_Input), TEXT("120.0"));
	UK2Node_IfThenElse* IsNear = OvBranch(G, 7100, 0);
	UK2Node_CallFunction* WeaponT = OvCall(G, AActor::StaticClass(), GET_FUNCTION_NAME_CHECKED(AActor, GetTransform), 7100, 250);
	UK2Node_VariableGet* OldGet3 = OvGetSelfVar(G, OldPointVar, 7100, 350);
	UK2Node_CallFunction* InvOld = OvCall(G, UKismetMathLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, InvertTransform), 7300, 350);
	UK2Node_CallFunction* Local = OvCall(G, UKismetMathLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, ComposeTransforms), 7500, 250);
	UK2Node_VariableGet* NewGet1 = OvGetSelfVar(G, NewPointVar, 7500, 400);
	UK2Node_CallFunction* WeaponNew = OvCall(G, UKismetMathLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, ComposeTransforms), 7700, 250);
	UK2Node_CallFunction* PlaceWeapon = OvCall(G, AActor::StaticClass(), GET_FUNCTION_NAME_CHECKED(AActor, K2_SetActorTransform), 7900, 0);
	OvSetValue(OvPin(PlaceWeapon, TEXT("bTeleport"), EGPD_Input), TEXT("true"));
	UK2Node_CallFunction* PlacePoint = OvCall(G, USceneComponent::StaticClass(), GET_FUNCTION_NAME_CHECKED(USceneComponent, K2_SetWorldTransform), 6250, -300);
	OvSetValue(OvPin(PlacePoint, TEXT("bTeleport"), EGPD_Input), TEXT("true"));
	UK2Node_VariableGet* NewGet2 = OvGetSelfVar(G, NewPointVar, 6000, -200);
	UK2Node_VariableSet* SetLast = OvSetSelfVar(G, LastTargetVar, 3150, -300);

	if (!TargetClass || !WeaponClass || !Find || !Valid || !Differs || !Both || !Comps || !CompLoop->GetMacroGraph() || !CompName || !Match || !CompWorld || !InvDefault || !Mine || !Delta || !Moved
		|| !Weapons || !WeaponLoop->GetMacroGraph() || !WeaponLoc || !BreakOld || !Dist || !Near || !WeaponT || !InvOld || !Local || !WeaponNew || !PlaceWeapon || !PlacePoint)
	{
		return nullptr;
	}

	OvLink(Exec, OvExec(Find));
	OvLink(TargetClass, OvPin(Find, TEXT("ActorClass"), EGPD_Input));
	OvLink(OvThen(Find), OvExec(NewTarget));
	OvLink(OvOut(Find), OvPin(Valid, TEXT("Object"), EGPD_Input));
	OvLink(OvOut(Find), OvPin(Differs, TEXT("A"), EGPD_Input));
	OvLink(OvPin(Last, LastTargetVar, EGPD_Output), OvPin(Differs, TEXT("B"), EGPD_Input));
	OvLink(OvOut(Valid), OvPin(Both, TEXT("A"), EGPD_Input));
	OvLink(OvOut(Differs), OvPin(Both, TEXT("B"), EGPD_Input));
	OvLink(OvOut(Both), NewTarget->GetConditionPin());
	OvLink(OvOut(Find), OvTarget(Comps));
	if (UEdGraphPin* CompsExec = Comps->FindPin(UEdGraphSchema_K2::PN_Execute, EGPD_Input))
	{
		OvLink(NewTarget->GetThenPin(), CompsExec);
		OvLink(OvThen(Comps), OvPin(CompLoop, TEXT("Exec"), EGPD_Input));
	}
	else
	{
		OvLink(NewTarget->GetThenPin(), OvPin(CompLoop, TEXT("Exec"), EGPD_Input));
	}
	OvLink(OvOut(Comps), OvPin(CompLoop, TEXT("Array"), EGPD_Input));
	UEdGraphPin* Comp = OvPin(CompLoop, TEXT("Array Element"), EGPD_Output);
	OvLink(OvLoopBody(CompLoop), OvExec(IsPoint));
	OvLink(Comp, OvPin(CompName, TEXT("Object"), EGPD_Input));
	OvLink(OvOut(CompName), OvPin(Match, TEXT("A"), EGPD_Input));
	OvLink(OvPin(Wanted, PointNameVar, EGPD_Output), OvPin(Match, TEXT("B"), EGPD_Input));
	OvLink(OvOut(Match), IsPoint->GetConditionPin());
	OvLink(IsPoint->GetThenPin(), OvExec(SetOld));
	OvLink(Comp, OvTarget(CompWorld));
	OvLink(OvOut(CompWorld), OvPin(SetOld, OldPointVar, EGPD_Input));
	OvLink(OvThen(SetOld), OvExec(SetNew));
	OvLink(OvPin(DefaultGet, PointDefaultVar, EGPD_Output), OvPin(InvDefault, TEXT("T"), EGPD_Input));
	OvLink(OvOut(InvDefault), OvPin(Delta, TEXT("A"), EGPD_Input));
	OvLink(OvOut(Mine), OvPin(Delta, TEXT("B"), EGPD_Input));
	OvLink(OvPin(OldGet1, OldPointVar, EGPD_Output), OvPin(Moved, TEXT("A"), EGPD_Input));
	OvLink(OvOut(Delta), OvPin(Moved, TEXT("B"), EGPD_Input));
	OvLink(OvOut(Moved), OvPin(SetNew, NewPointVar, EGPD_Input));
	OvLink(WeaponExec, OvExec(Weapons));
	OvLink(WeaponClass, OvPin(Weapons, TEXT("ActorClass"), EGPD_Input));
	OvLink(OvThen(Weapons), OvPin(WeaponLoop, TEXT("Exec"), EGPD_Input));
	OvLink(OvPin(Weapons, TEXT("OutActors"), EGPD_Output), OvPin(WeaponLoop, TEXT("Array"), EGPD_Input));
	UEdGraphPin* Weapon = OvPin(WeaponLoop, TEXT("Array Element"), EGPD_Output);
	OvLink(OvLoopBody(WeaponLoop), OvExec(IsNear));
	OvLink(Weapon, OvTarget(WeaponLoc));
	OvLink(OvPin(OldGet2, OldPointVar, EGPD_Output), OvPin(BreakOld, TEXT("InTransform"), EGPD_Input));
	OvLink(OvOut(WeaponLoc), OvPin(Dist, TEXT("V1"), EGPD_Input));
	OvLink(OvPin(BreakOld, TEXT("Location"), EGPD_Output), OvPin(Dist, TEXT("V2"), EGPD_Input));
	OvLink(OvOut(Dist), OvPin(Near, TEXT("A"), EGPD_Input));
	OvLink(OvOut(Near), IsNear->GetConditionPin());
	OvLink(IsNear->GetThenPin(), OvExec(PlaceWeapon));
	OvLink(Weapon, OvTarget(WeaponT));
	OvLink(OvPin(OldGet3, OldPointVar, EGPD_Output), OvPin(InvOld, TEXT("T"), EGPD_Input));
	OvLink(OvOut(WeaponT), OvPin(Local, TEXT("A"), EGPD_Input));
	OvLink(OvOut(InvOld), OvPin(Local, TEXT("B"), EGPD_Input));
	OvLink(OvOut(Local), OvPin(WeaponNew, TEXT("A"), EGPD_Input));
	OvLink(OvPin(NewGet1, NewPointVar, EGPD_Output), OvPin(WeaponNew, TEXT("B"), EGPD_Input));
	OvLink(OvOut(WeaponNew), OvPin(PlaceWeapon, TEXT("NewTransform"), EGPD_Input));
	OvLink(Weapon, OvTarget(PlaceWeapon));
	OvLink(OvPin(WeaponLoop, TEXT("Completed"), EGPD_Output), OvExec(PlacePoint));
	OvLink(WeaponFail, OvExec(PlacePoint));
	OvLink(Comp, OvTarget(PlacePoint));
	OvLink(OvPin(NewGet2, NewPointVar, EGPD_Output), OvPin(PlacePoint, TEXT("NewTransform"), EGPD_Input));
	OvLink(OvPin(CompLoop, TEXT("Completed"), EGPD_Output), OvExec(SetLast));
	OvLink(OvOut(Find), OvPin(SetLast, LastTargetVar, EGPD_Input));

	OvFinishBlueprint(BP);
	return BP;
}

int32 UAEWArenaTools::AddWeaponPointMarkers()
{
	UWorld* World = OvEditorWorld();
	if (!World)
	{
		return 0;
	}
	UBlueprint* BP = EnsureWeaponMarkerBlueprint(OvArenaFolderOf(World), true);
	UClass* MarkerClass = BP ? BP->GeneratedClass.Get() : nullptr;
	FStrProperty* PathProp = MarkerClass ? FindFProperty<FStrProperty>(MarkerClass, TargetPathVar) : nullptr;
	FStrProperty* PointProp = MarkerClass ? FindFProperty<FStrProperty>(MarkerClass, PointNameVar) : nullptr;
	FStructProperty* DefaultProp = MarkerClass ? FindFProperty<FStructProperty>(MarkerClass, PointDefaultVar) : nullptr;
	if (!PathProp || !PointProp || !DefaultProp)
	{
		OvReport(TEXT("Could not create BP_WeaponPointMarker - see the Output Log."));
		return 0;
	}

	FTransform Owner = FTransform::Identity;
	bool bRing = false;
	if (AActor* Ring = OvFindActorOfClassName(World, TEXT("BP_Ring_C")))
	{
		Owner = FTransform(Ring->GetActorRotation(), Ring->GetActorLocation());
		bRing = true;
	}

	TSet<FString> Existing;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->GetClass() == MarkerClass)
		{
			Existing.Add(PathProp->GetPropertyValue_InContainer(*It) + TEXT("|") + PointProp->GetPropertyValue_InContainer(*It));
		}
	}

	const FScopedTransaction Transaction(LOCTEXT("AddWeaponPointMarkers", "Add Weapon Point Markers"));
	World->PersistentLevel->Modify();
	int32 Added = 0;
	for (const FOvWeaponPoint& P : WeaponPoints)
	{
		const FString Path = OvWeaponOwnerPath(P.ClassName);
		if (Existing.Contains(Path + TEXT("|") + P.Point))
		{
			continue;
		}
		const FTransform World_ = FTransform(P.Rotation, P.Location) * Owner;
		FActorSpawnParameters Params;
		Params.OverrideLevel = World->PersistentLevel;
		AActor* Marker = World->SpawnActor<AActor>(MarkerClass, World_, Params);
		if (!Marker)
		{
			continue;
		}
		PathProp->SetPropertyValue_InContainer(Marker, Path);
		PointProp->SetPropertyValue_InContainer(Marker, P.Point);
		*DefaultProp->ContainerPtrToValuePtr<FTransform>(Marker) = World_;
		const bool bLightsOut = FCString::Strcmp(P.ClassName, TEXT("BP_LightsOutWeaponGenerator_C")) == 0;
		Marker->SetActorLabel(FString(bLightsOut ? TEXT("LightsOut_") : TEXT("Placement_")) + P.Point);
		Marker->SetFolderPath(bLightsOut ? TEXT("ArenaOverrides/WeaponPoints/LightsOut") : TEXT("ArenaOverrides/WeaponPoints/LadderAndTacks"));
		++Added;
	}
	World->PersistentLevel->MarkPackageDirty();
	OvReport(FString::Printf(TEXT("Added %d weapon point marker(s) (%d already there)%s.\n\n")
		TEXT("Move the ones to change, then save. Unmoved markers change nothing."),
		Added, Existing.Num(), bRing ? TEXT(", placed around BP_Ring") : TEXT(", around the world origin (no BP_Ring)")));
	return Added;
}

bool UAEWArenaTools::AddMatchRuleOverrideNodes()
{
	UWorld* World = OvEditorWorld();
	if (!World)
	{
		return false;
	}
	UClass* Interface = LoadObject<UClass>(nullptr, GameInstanceInterfacePath);
	UUserDefinedStruct* RuleStruct = LoadObject<UUserDefinedStruct>(nullptr, MatchRuleStructPath);
	UFunction* GetRule = Interface ? Interface->FindFunctionByName(TEXT("GetMatchRule")) : nullptr;
	UFunction* SetRule = Interface ? Interface->FindFunctionByName(TEXT("SetMatchRule")) : nullptr;
	if (!GetRule || !SetRule || !RuleStruct)
	{
		OvReport(FString::Printf(TEXT("Couldn't load the match rule interface or struct:\n%s %s\n%s %s"),
			GameInstanceInterfacePath, (GetRule && SetRule) ? TEXT("ok") : TEXT("missing"), MatchRuleStructPath, RuleStruct ? TEXT("ok") : TEXT("missing")));
		return false;
	}
	ULevelScriptBlueprint* LBP = World->PersistentLevel->GetLevelScriptBlueprint(false);
	if (!LBP)
	{
		return false;
	}
	UEdGraph* G = FBlueprintEditorUtils::FindEventGraph(LBP);
	if (!G)
	{
		return false;
	}
	const FScopedTransaction Transaction(LOCTEXT("AddRuleNodes", "Add Match Rule Override"));
	LBP->Modify();
	G->Modify();

	if (FBlueprintEditorUtils::FindNewVariableIndex(LBP, OriginalRuleVar) == INDEX_NONE)
	{
		FEdGraphPinType Type;
		Type.PinCategory = UEdGraphSchema_K2::PC_Struct;
		Type.PinSubCategoryObject = RuleStruct;
		FBlueprintEditorUtils::AddMemberVariable(LBP, OriginalRuleVar, Type);
	}

	const int32 Y = OvLowestY(G);
	UK2Node_Event* Begin = OvEventNode(G, TEXT("ReceiveBeginPlay"), 0, Y);
	UK2Node_CallFunction* GI1 = OvCall(G, UGameplayStatics::StaticClass(), GET_FUNCTION_NAME_CHECKED(UGameplayStatics, GetGameInstance), 250, Y + 150);
	UK2Node_Message* Get = OvMessage(G, GetRule, 500, Y);
	UK2Node_VariableSet* Save = NewObject<UK2Node_VariableSet>(G);
	Save->VariableReference.SetSelfMember(OriginalRuleVar);
	OvPlace(G, Save, 800, Y);
	UK2Node_SetFieldsInStruct* Fields = NewObject<UK2Node_SetFieldsInStruct>(G);
	Fields->StructType = RuleStruct;
	OvPlace(G, Fields, 1100, Y);
	UK2Node_CallFunction* GI2 = OvCall(G, UGameplayStatics::StaticClass(), GET_FUNCTION_NAME_CHECKED(UGameplayStatics, GetGameInstance), 1150, Y + 450);
	UK2Node_Message* Set = OvMessage(G, SetRule, 1500, Y);

	UEdGraphPin* GetOut = OvPinOfType(Get, EGPD_Output, RuleStruct);
	OvLink(OvOut(GI1), OvTarget(Get));
	OvLink(OvThen(Get), OvExec(Save));
	OvLink(GetOut, OvPin(Save, OriginalRuleVar, EGPD_Input));
	OvLink(OvThen(Save), OvExec(Fields));
	OvLink(GetOut, OvPinOfType(Fields, EGPD_Input, RuleStruct));
	OvLink(OvThen(Fields), OvExec(Set));
	OvLink(OvOut(GI2), OvTarget(Set));
	OvLink(OvPinOfType(Fields, EGPD_Output, RuleStruct), OvPinOfType(Set, EGPD_Input, RuleStruct));
	OvInsertAfter(OvThen(Begin), OvExec(Get), OvThen(Set));

	UK2Node_Event* End = OvEventNode(G, TEXT("ReceiveEndPlay"), 0, Y + 700);
	UK2Node_CallFunction* GI3 = OvCall(G, UGameplayStatics::StaticClass(), GET_FUNCTION_NAME_CHECKED(UGameplayStatics, GetGameInstance), 250, Y + 850);
	UK2Node_VariableGet* Saved = OvGetSelfVar(G, OriginalRuleVar, 250, Y + 950);
	UK2Node_Message* Restore = OvMessage(G, SetRule, 500, Y + 700);
	OvLink(OvOut(GI3), OvTarget(Restore));
	OvLink(OvPin(Saved, OriginalRuleVar, EGPD_Output), OvPinOfType(Restore, EGPD_Input, RuleStruct));
	OvInsertAfter(OvThen(End), OvExec(Restore), OvThen(Restore));

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(LBP);
	FKismetEditorUtilities::CompileBlueprint(LBP);
	World->PersistentLevel->MarkPackageDirty();
	const bool bOk = LBP->Status != BS_Error;
	OvReport(bOk
		? TEXT("Added a match rule override to the Level Blueprint.\n\nTick the fields to change on the Set members node, set them, compile and save.")
		: TEXT("Nodes added, but the Level Blueprint has compile errors."));
	return bOk;
}

void UAEWArenaTools::ToggleCameraPreview()
{
	UWorld* World = OvEditorWorld();
	if (!World)
	{
		return;
	}
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->ActorHasTag(CameraPreviewTag))
		{
			ClearCameraPreview();
			return;
		}
	}
	PreviewGameplayCamera();
}

namespace
{
	struct FOvRemoveGroup
	{
		FString Label;
		TArray<FString> ClassNames;
		TArray<FString> AssetPrefixes;
		TArray<AActor*> Actors;
		TArray<FAssetData> Assets;
		int32 RuleNodes = 0;
		bool bOn = true;

		int32 Found() const
		{
			return Actors.Num() + Assets.Num() + RuleNodes;
		}
	};

	bool OvIsRuleNode(UEdGraphNode* Node, UUserDefinedStruct* RuleStruct)
	{
		if (UK2Node_Message* M = Cast<UK2Node_Message>(Node))
		{
			const FName Name = M->FunctionReference.GetMemberName();
			return Name == TEXT("GetMatchRule") || Name == TEXT("SetMatchRule");
		}
		if (UK2Node_SetFieldsInStruct* F = Cast<UK2Node_SetFieldsInStruct>(Node))
		{
			return RuleStruct && F->StructType == RuleStruct;
		}
		if (UK2Node_Variable* V = Cast<UK2Node_Variable>(Node))
		{
			return V->VariableReference.GetMemberName() == OriginalRuleVar;
		}
		return false;
	}

	void OvCollectExecTargets(UEdGraphNode* Node, const TSet<UEdGraphNode*>& Removing, TSet<UEdGraphNode*>& Visited, TArray<UEdGraphPin*>& Out)
	{
		if (Visited.Contains(Node))
		{
			return;
		}
		Visited.Add(Node);
		for (UEdGraphPin* P : Node->Pins)
		{
			if (!P || P->Direction != EGPD_Output || P->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
			{
				continue;
			}
			for (UEdGraphPin* L : P->LinkedTo)
			{
				if (Removing.Contains(L->GetOwningNode()))
				{
					OvCollectExecTargets(L->GetOwningNode(), Removing, Visited, Out);
				}
				else
				{
					Out.AddUnique(L);
				}
			}
		}
	}

	TSet<UEdGraphNode*> OvFindRuleNodes(ULevelScriptBlueprint* LBP)
	{
		TSet<UEdGraphNode*> Removing;
		if (!LBP)
		{
			return Removing;
		}
		UUserDefinedStruct* RuleStruct = LoadObject<UUserDefinedStruct>(nullptr, MatchRuleStructPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
		TArray<UEdGraph*> Graphs;
		LBP->GetAllGraphs(Graphs);
		for (UEdGraph* G : Graphs)
		{
			for (UEdGraphNode* Node : G->Nodes)
			{
				if (Node && OvIsRuleNode(Node, RuleStruct))
				{
					Removing.Add(Node);
				}
			}
		}
		for (UEdGraph* G : Graphs)
		{
			for (UEdGraphNode* Node : G->Nodes)
			{
				UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node);
				if (!Call || Removing.Contains(Node) || Call->FunctionReference.GetMemberName() != GET_FUNCTION_NAME_CHECKED(UGameplayStatics, GetGameInstance))
				{
					continue;
				}
				bool bAny = false;
				bool bOnlyRule = true;
				for (UEdGraphPin* P : Call->Pins)
				{
					for (UEdGraphPin* L : P ? P->LinkedTo : TArray<UEdGraphPin*>())
					{
						bAny = true;
						bOnlyRule = bOnlyRule && Removing.Contains(L->GetOwningNode());
					}
				}
				if (bAny && bOnlyRule)
				{
					Removing.Add(Node);
				}
			}
		}
		return Removing;
	}

	int32 OvRemoveRuleNodes(ULevelScriptBlueprint* LBP)
	{
		const TSet<UEdGraphNode*> Removing = OvFindRuleNodes(LBP);
		const bool bHasVar = LBP && FBlueprintEditorUtils::FindNewVariableIndex(LBP, OriginalRuleVar) != INDEX_NONE;
		if (Removing.Num() == 0 && !bHasVar)
		{
			return 0;
		}
		LBP->Modify();
		for (UEdGraphNode* Node : Removing)
		{
			for (UEdGraphPin* In : Node->Pins)
			{
				if (!In || In->Direction != EGPD_Input || In->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
				{
					continue;
				}
				TArray<UEdGraphPin*> Sources;
				for (UEdGraphPin* L : In->LinkedTo)
				{
					if (!Removing.Contains(L->GetOwningNode()))
					{
						Sources.Add(L);
					}
				}
				if (Sources.Num() == 0)
				{
					continue;
				}
				TSet<UEdGraphNode*> Visited;
				TArray<UEdGraphPin*> Targets;
				OvCollectExecTargets(Node, Removing, Visited, Targets);
				for (UEdGraphPin* Source : Sources)
				{
					Source->GetOwningNode()->Modify();
					Source->BreakLinkTo(In);
					for (UEdGraphPin* Target : Targets)
					{
						OvLink(Source, Target);
					}
				}
			}
		}
		for (UEdGraphNode* Node : Removing)
		{
			FBlueprintEditorUtils::RemoveNode(LBP, Node, true);
		}
		if (bHasVar)
		{
			FBlueprintEditorUtils::RemoveMemberVariable(LBP, OriginalRuleVar);
		}
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(LBP);
		FKismetEditorUtilities::CompileBlueprint(LBP);
		return Removing.Num();
	}

	bool OvAskRemove(TArray<FOvRemoveGroup>& Groups, bool& bDeleteAssets, const FString& MapName, const FString& Folder)
	{
		bool bOk = false;
		TSharedRef<SWindow> Window = SNew(SWindow)
			.Title(LOCTEXT("RemoveTitle", "Remove Overrides"))
			.SizingRule(ESizingRule::Autosized)
			.SupportsMinimize(false)
			.SupportsMaximize(false);
		TWeakPtr<SWindow> WeakWindow = Window;
		TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
		Box->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
		[
			SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("Remove these overrides from %s:"), *MapName)))
		];
		for (FOvRemoveGroup& G : Groups)
		{
			FOvRemoveGroup* Ptr = &G;
			FString Label = G.Label;
			if (G.Found() == 0)
			{
				Label += TEXT("  (none)");
			}
			else
			{
				TArray<FString> Parts;
				if (G.Actors.Num() > 0)
				{
					Parts.Add(FString::Printf(TEXT("%d actor(s)"), G.Actors.Num()));
				}
				if (G.RuleNodes > 0)
				{
					Parts.Add(FString::Printf(TEXT("%d node(s)"), G.RuleNodes));
				}
				if (G.Assets.Num() > 0)
				{
					Parts.Add(FString::Printf(TEXT("%d asset(s)"), G.Assets.Num()));
				}
				Label += TEXT("  (") + FString::Join(Parts, TEXT(", ")) + TEXT(")");
			}
			G.bOn = G.Found() > 0;
			Box->AddSlot().AutoHeight().Padding(0.f, 2.f)
			[
				SNew(SCheckBox)
				.IsEnabled(G.Found() > 0)
				.IsChecked_Lambda([Ptr]() { return Ptr->bOn ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([Ptr](ECheckBoxState State) { Ptr->bOn = State == ECheckBoxState::Checked; })
				[
					SNew(STextBlock).Text(FText::FromString(Label))
				]
			];
		}
		bool* DeletePtr = &bDeleteAssets;
		Box->AddSlot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f)
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([DeletePtr]() { return *DeletePtr ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			.OnCheckStateChanged_Lambda([DeletePtr](ECheckBoxState State) { *DeletePtr = State == ECheckBoxState::Checked; })
			.ToolTipText(LOCTEXT("RemoveAssetsTip", "The map is saved first."))
			[
				SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("Also delete their blueprints and camera table from %s"), *Folder)))
			]
		];
		bool* OkPtr = &bOk;
		Box->AddSlot().AutoHeight().HAlign(HAlign_Right).Padding(0.f, 12.f, 0.f, 0.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 6.f, 0.f)
			[
				SNew(SButton)
				.Text(LOCTEXT("RemoveButton", "Remove"))
				.OnClicked_Lambda([OkPtr, WeakWindow]()
				{
					*OkPtr = true;
					if (TSharedPtr<SWindow> W = WeakWindow.Pin())
					{
						W->RequestDestroyWindow();
					}
					return FReply::Handled();
				})
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("CancelButton", "Cancel"))
				.OnClicked_Lambda([WeakWindow]()
				{
					if (TSharedPtr<SWindow> W = WeakWindow.Pin())
					{
						W->RequestDestroyWindow();
					}
					return FReply::Handled();
				})
			]
		];
		Window->SetContent(SNew(SBorder).BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder")).Padding(12.f)[ Box ]);
		GEditor->EditorAddModalWindow(Window);
		return bOk;
	}
}

int32 UAEWArenaTools::RemoveOverrides()
{
	UWorld* World = OvEditorWorld();
	if (!World)
	{
		return 0;
	}
	const FString Folder = OvArenaFolderOf(World);
	const FString MapName = FPackageName::GetShortName(World->GetOutermost()->GetName());

	TArray<FOvRemoveGroup> Groups;
	Groups.AddDefaulted(5);
	Groups[0].Label = TEXT("Start markers");
	Groups[0].ClassNames = { FString(StartMarkerName) + TEXT("_C") };
	Groups[0].AssetPrefixes = { StartMarkerName };
	Groups[1].Label = TEXT("Camera override and camera preview");
	Groups[1].ClassNames = { FString(CameraOverrideName) + TEXT("_C") };
	Groups[1].AssetPrefixes = { CameraOverrideName, TEXT("DT_GameplayCameraParam") };
	Groups[2].Label = TEXT("Entrance override and its entrance swaps");
	Groups[2].ClassNames = { FString(EntranceOverrideName) + TEXT("_C") };
	Groups[2].AssetPrefixes = { EntranceOverrideName };
	Groups[3].Label = TEXT("Weapon point markers");
	Groups[3].ClassNames = { FString(WeaponMarkerName) + TEXT("_C") };
	Groups[3].AssetPrefixes = { WeaponMarkerName };
	Groups[4].Label = TEXT("Rule override (Level Blueprint nodes and OriginalMatchRule)");

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		const FString ClassName = It->GetClass()->GetName();
		for (int32 i = 0; i < 4; ++i)
		{
			if (Groups[i].ClassNames.Contains(ClassName) || (i == 1 && It->ActorHasTag(CameraPreviewTag)))
			{
				Groups[i].Actors.Add(*It);
			}
		}
	}
	IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	TArray<FAssetData> FolderAssets;
	AR.GetAssetsByPath(FName(*Folder), FolderAssets, false);
	for (const FAssetData& A : FolderAssets)
	{
		const FString Name = A.AssetName.ToString();
		for (int32 i = 0; i < 4; ++i)
		{
			for (const FString& Prefix : Groups[i].AssetPrefixes)
			{
				if (Name == Prefix || (Prefix.StartsWith(TEXT("DT_")) && Name.StartsWith(Prefix)))
				{
					Groups[i].Assets.Add(A);
				}
			}
		}
	}
	ULevelScriptBlueprint* LBP = World->PersistentLevel->GetLevelScriptBlueprint(true);
	Groups[4].RuleNodes = OvFindRuleNodes(LBP).Num();
	if (Groups[4].RuleNodes == 0 && LBP && FBlueprintEditorUtils::FindNewVariableIndex(LBP, OriginalRuleVar) != INDEX_NONE)
	{
		Groups[4].RuleNodes = 1;
	}

	int32 Total = 0;
	for (const FOvRemoveGroup& G : Groups)
	{
		Total += G.Found();
	}
	if (Total == 0)
	{
		OvReport(FString::Printf(TEXT("No arena overrides found in %s or %s."), *MapName, *Folder));
		return 0;
	}
	bool bDeleteAssets = true;
	if (!OvAskRemove(Groups, bDeleteAssets, MapName, Folder))
	{
		return 0;
	}

	int32 Actors = 0;
	int32 Nodes = 0;
	{
		const FScopedTransaction Transaction(LOCTEXT("RemoveOverrides", "Remove Arena Overrides"));
		World->PersistentLevel->Modify();
		for (int32 i = 0; i < 4; ++i)
		{
			if (!Groups[i].bOn)
			{
				continue;
			}
			for (AActor* A : Groups[i].Actors)
			{
				if (IsValid(A) && World->EditorDestroyActor(A, true))
				{
					++Actors;
				}
			}
		}
		if (Groups[1].bOn)
		{
			FlushPersistentDebugLines(World);
		}
		if (Groups[4].bOn)
		{
			Nodes = OvRemoveRuleNodes(LBP);
		}
		World->PersistentLevel->MarkPackageDirty();
	}
	if (Groups[2].bOn)
	{
		UpdateEntranceSwaps(false);
	}

	TArray<UObject*> ToDelete;
	if (bDeleteAssets)
	{
		for (const FOvRemoveGroup& G : Groups)
		{
			for (const FAssetData& A : G.bOn ? G.Assets : TArray<FAssetData>())
			{
				if (UObject* Obj = A.GetAsset())
				{
					ToDelete.Add(Obj);
				}
			}
		}
	}
	int32 Deleted = 0;
	if (ToDelete.Num() > 0)
	{
		UEditorLoadingAndSavingUtils::SavePackages({ World->GetOutermost() }, false);
		Deleted = ObjectTools::DeleteObjects(ToDelete, true);
	}
	GEditor->RedrawLevelEditingViewports();

	FString Message = FString::Printf(TEXT("Removed %d actor(s)"), Actors);
	if (Groups[4].bOn)
	{
		Message += FString::Printf(TEXT(" and %d rule override node(s)"), Nodes);
	}
	Message += FString::Printf(TEXT(" from %s."), *MapName);
	if (ToDelete.Num() > 0)
	{
		Message += FString::Printf(TEXT("\nDeleted %d of %d asset(s) from %s."), Deleted, ToDelete.Num(), *Folder);
	}
	if (Groups[2].bOn)
	{
		Message += TEXT("\nRemoved this arena's entrance swaps.");
	}
	if (ToDelete.Num() == 0)
	{
		Message += TEXT("\n\nSave the map to keep the change.");
	}
	OvReport(Message);
	return Actors + Nodes + Deleted;
}

namespace
{
	const TCHAR* BarricadeGuardName = TEXT("BP_BarricadeGuard");
	const FName GuardPiecesVar(TEXT("Pieces"));
	const FName GuardMeshesVar(TEXT("Meshes"));
	const FName GuardKnockVar(TEXT("bCanBeKnockedDown"));
	const FName GuardDownVar(TEXT("bStartsDown"));
	const FName GuardClimbVar(TEXT("bCanBeClimbed"));
	const FName GuardMovesVar(TEXT("bAllowBarricadeMoves"));
	const FName NoClimbVar(TEXT("\u767b\u308c\u306a\u3044"));
	const FName GuardCheckEvent(TEXT("CheckBarricades"));
	const FName ConditionVar(TEXT("BarricadeCondition"));

	UEdGraphPin* OvObjectOut(UEdGraphNode* Node)
	{
		for (UEdGraphPin* P : Node->Pins)
		{
			if (P && P->Direction == EGPD_Output && P->PinType.PinCategory == UEdGraphSchema_K2::PC_Object)
			{
				return P;
			}
		}
		return nullptr;
	}

	FString OvEnumName(UClass* Class, const FName Prop, int64 Value)
	{
		UEnum* Enum = nullptr;
		if (FByteProperty* B = FindFProperty<FByteProperty>(Class, Prop))
		{
			Enum = B->Enum;
		}
		else if (FEnumProperty* E = FindFProperty<FEnumProperty>(Class, Prop))
		{
			Enum = E->GetEnum();
		}
		return Enum ? Enum->GetNameStringByValue(Value) : FString::FromInt(int32(Value));
	}
}

UBlueprint* UAEWArenaTools::EnsureBarricadeGuardBlueprint(const FString& ArenaFolder, UClass* PieceClass, bool bRebuild)
{
	if (!PieceClass || !PieceClass->FindPropertyByName(ConditionVar) || !PieceClass->FindFunctionByName(TEXT("ResetShake")))
	{
		UE_LOG(LogAEWArenaOverrides, Error, TEXT("%s has no BarricadeCondition / ResetShake; can't build the barricade guard."), PieceClass ? *PieceClass->GetName() : TEXT("(no class)"));
		return nullptr;
	}
	bool bNew = false;
	UBlueprint* BP = OvNewActorBlueprint(ArenaFolder, BarricadeGuardName, bRebuild, bNew);
	if (!BP || (!bNew && !bRebuild))
	{
		return BP;
	}
	FEdGraphPinType ActorArray;
	ActorArray.PinCategory = UEdGraphSchema_K2::PC_Object;
	ActorArray.PinSubCategoryObject = AActor::StaticClass();
	ActorArray.ContainerType = EPinContainerType::Array;
	FEdGraphPinType BoolType;
	BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
	OvAddVar(BP, GuardPiecesVar, ActorArray, true, TEXT("The invisible barricade pieces this guard looks after."));
	OvAddVar(BP, GuardMeshesVar, ActorArray, true, TEXT("Mesh actors hidden when the barricade is knocked down."));
	OvAddVar(BP, GuardKnockVar, BoolType, true, TEXT("Moves can knock this barricade down."));
	OvAddVar(BP, GuardDownVar, BoolType, true, TEXT("Starts the match knocked down."));
	OvAddVar(BP, GuardClimbVar, BoolType, true, TEXT("Wrestlers can climb onto this barricade."));
	OvAddVar(BP, GuardMovesVar, BoolType, true, TEXT("Wrestlers can use this barricade. Off = a plain wall."));
	const int32 MovesIndex = FBlueprintEditorUtils::FindNewVariableIndex(BP, GuardMovesVar);
	if (MovesIndex != INDEX_NONE)
	{
		BP->NewVariables[MovesIndex].DefaultValue = TEXT("true");
	}
	const int32 KnockIndex = FBlueprintEditorUtils::FindNewVariableIndex(BP, GuardKnockVar);
	if (KnockIndex != INDEX_NONE)
	{
		BP->NewVariables[KnockIndex].DefaultValue = TEXT("true");
	}

	UEdGraph* G = FBlueprintEditorUtils::FindEventGraph(BP);
	UK2Node_Event* Begin = OvBeginPlayNode(G, 0, 0);
	UK2Node_CallFunction* Timer = OvCall(G, UKismetSystemLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, K2_SetTimer), 300, 0);
	if (!Timer)
	{
		return nullptr;
	}
	OvSetValue(OvPin(Timer, TEXT("FunctionName"), EGPD_Input), GuardCheckEvent.ToString());
	OvSetValue(OvPin(Timer, TEXT("Time"), EGPD_Input), TEXT("0.1"));
	OvSetValue(OvPin(Timer, TEXT("bLooping"), EGPD_Input), TEXT("true"));
	OvLink(OvThen(Begin), OvExec(Timer));

	UK2Node_CustomEvent* Check = NewObject<UK2Node_CustomEvent>(G);
	Check->CustomFunctionName = GuardCheckEvent;
	OvPlace(G, Check, 0, 300);
	UK2Node_VariableGet* PiecesGet = OvGetSelfVar(G, GuardPiecesVar, 0, 500);
	UK2Node_MacroInstance* Loop = NewObject<UK2Node_MacroInstance>(G);
	Loop->SetMacroGraph(OvForEachMacro());
	OvPlace(G, Loop, 300, 300);
	UK2Node_DynamicCast* AsPiece = NewObject<UK2Node_DynamicCast>(G);
	AsPiece->TargetType = PieceClass;
	OvPlace(G, AsPiece, 600, 300);
	UK2Node_VariableGet* Cond = OvGetMember(G, PieceClass, ConditionVar, 900, 600);
	const bool bHasClimb = PieceClass->FindPropertyByName(NoClimbVar) != nullptr;
	UK2Node_VariableSet* SetClimb = bHasClimb ? OvSetMember(G, PieceClass, NoClimbVar, 850, 100) : nullptr;
	UK2Node_VariableGet* ClimbGet = OvGetSelfVar(G, GuardClimbVar, 600, 150);
	UK2Node_CallFunction* NoClimb = OvCall(G, UKismetMathLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Not_PreBool), 700, 100);
	UK2Node_IfThenElse* IsDownMode = OvBranch(G, 1050, 300);
	UK2Node_IfThenElse* MovesOn = OvBranch(G, 1000, -150);
	UK2Node_VariableGet* MovesGet = OvGetSelfVar(G, GuardMovesVar, 800, -50);
	const bool bHasZones = PieceClass->FindPropertyByName(TEXT("CollisionReactionArea")) && PieceClass->FindPropertyByName(TEXT("PlayerHitCollisionOverlapCheck"));
	UK2Node_VariableGet* ReactGet = bHasZones ? OvGetMember(G, PieceClass, TEXT("CollisionReactionArea"), 1200, -50) : nullptr;
	UK2Node_VariableGet* HitGet = bHasZones ? OvGetMember(G, PieceClass, TEXT("PlayerHitCollisionOverlapCheck"), 1450, -50) : nullptr;
	UK2Node_CallFunction* ReactOff = bHasZones ? OvCall(G, UPrimitiveComponent::StaticClass(), GET_FUNCTION_NAME_CHECKED(UPrimitiveComponent, SetCollisionEnabled), 1250, -200) : nullptr;
	UK2Node_CallFunction* HitOff = bHasZones ? OvCall(G, UPrimitiveComponent::StaticClass(), GET_FUNCTION_NAME_CHECKED(UPrimitiveComponent, SetCollisionEnabled), 1500, -200) : nullptr;
	OvSetValue(ReactOff ? OvPin(ReactOff, TEXT("NewType"), EGPD_Input) : nullptr, TEXT("NoCollision"));
	OvSetValue(HitOff ? OvPin(HitOff, TEXT("NewType"), EGPD_Input) : nullptr, TEXT("NoCollision"));
	UK2Node_VariableGet* DownGet = OvGetSelfVar(G, GuardDownVar, 700, 500);
	UK2Node_VariableSet* SetDown = OvSetMember(G, PieceClass, ConditionVar, 1200, 200);
	OvSetValue(OvPin(SetDown, ConditionVar, EGPD_Input), OvEnumName(PieceClass, ConditionVar, 2));
	UK2Node_IfThenElse* CanKnock = OvBranch(G, 1200, 450);
	UK2Node_VariableGet* KnockGet = OvGetSelfVar(G, GuardKnockVar, 1000, 700);
	UK2Node_CallFunction* NotStanding = OvCall(G, UKismetMathLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, NotEqual_ByteByte), 1250, 800);
	OvSetValue(OvPin(NotStanding, TEXT("B"), EGPD_Input), TEXT("0"));
	UK2Node_IfThenElse* Knocked = OvBranch(G, 1500, 650);
	UK2Node_CallFunction* Reset = OvCall(G, PieceClass, TEXT("ResetShake"), 1750, 650);
	UK2Node_CallFunction* IsFallen = OvCall(G, UKismetMathLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, EqualEqual_ByteByte), 1250, 1000);
	OvSetValue(OvPin(IsFallen, TEXT("B"), EGPD_Input), TEXT("2"));
	UK2Node_IfThenElse* Fallen = OvBranch(G, 1500, 400);
	UK2Node_VariableGet* MeshesGet = OvGetSelfVar(G, GuardMeshesVar, 1600, 550);
	UK2Node_MacroInstance* MeshLoop = NewObject<UK2Node_MacroInstance>(G);
	MeshLoop->SetMacroGraph(OvForEachMacro());
	OvPlace(G, MeshLoop, 1800, 400);
	UK2Node_CallFunction* Hide = OvCall(G, AActor::StaticClass(), GET_FUNCTION_NAME_CHECKED(AActor, SetActorHiddenInGame), 2100, 400);
	UK2Node_CallFunction* NoCollision = OvCall(G, AActor::StaticClass(), GET_FUNCTION_NAME_CHECKED(AActor, SetActorEnableCollision), 2350, 400);
	if (!NotStanding || !Reset || !IsFallen || !Hide || !NoCollision)
	{
		return nullptr;
	}
	OvSetValue(OvPin(Hide, TEXT("bNewHidden"), EGPD_Input), TEXT("true"));
	OvSetValue(OvPin(NoCollision, TEXT("bNewActorEnableCollision"), EGPD_Input), TEXT("false"));

	UEdGraphPin* Piece = OvObjectOut(AsPiece);
	OvLink(OvThen(Check), OvPin(Loop, TEXT("Exec"), EGPD_Input));
	OvLink(OvPin(PiecesGet, GuardPiecesVar, EGPD_Output), OvPin(Loop, TEXT("Array"), EGPD_Input));
	OvLink(OvLoopBody(Loop), OvExec(AsPiece));
	OvLink(OvPin(Loop, TEXT("Array Element"), EGPD_Output), OvPin(AsPiece, TEXT("Object"), EGPD_Input));
	OvLink(Piece, OvTarget(Cond));
	UEdGraphPin* Next = OvThen(AsPiece);
	if (SetClimb && NoClimb)
	{
		OvLink(Next, OvExec(SetClimb));
		OvLink(Piece, OvTarget(SetClimb));
		OvLink(OvPin(ClimbGet, GuardClimbVar, EGPD_Output), OvPin(NoClimb, TEXT("A"), EGPD_Input));
		OvLink(OvOut(NoClimb), OvPin(SetClimb, NoClimbVar, EGPD_Input));
		Next = OvThen(SetClimb);
	}
	if (ReactOff && HitOff && ReactGet && HitGet)
	{
		OvLink(Next, OvExec(MovesOn));
		OvLink(OvPin(MovesGet, GuardMovesVar, EGPD_Output), MovesOn->GetConditionPin());
		OvLink(MovesOn->GetThenPin(), OvExec(IsDownMode));
		OvLink(MovesOn->GetElsePin(), OvExec(ReactOff));
		OvLink(Piece, OvTarget(ReactGet));
		OvLink(OvPin(ReactGet, TEXT("CollisionReactionArea"), EGPD_Output), OvTarget(ReactOff));
		OvLink(OvThen(ReactOff), OvExec(HitOff));
		OvLink(Piece, OvTarget(HitGet));
		OvLink(OvPin(HitGet, TEXT("PlayerHitCollisionOverlapCheck"), EGPD_Output), OvTarget(HitOff));
		OvLink(OvThen(HitOff), OvExec(IsDownMode));
	}
	else
	{
		OvLink(Next, OvExec(IsDownMode));
	}
	OvLink(OvPin(DownGet, GuardDownVar, EGPD_Output), IsDownMode->GetConditionPin());
	OvLink(IsDownMode->GetThenPin(), OvExec(SetDown));
	OvLink(Piece, OvTarget(SetDown));
	OvLink(IsDownMode->GetElsePin(), OvExec(CanKnock));
	OvLink(OvPin(KnockGet, GuardKnockVar, EGPD_Output), CanKnock->GetConditionPin());
	OvLink(OvPin(Cond, ConditionVar, EGPD_Output), OvPin(NotStanding, TEXT("A"), EGPD_Input));
	OvLink(OvOut(NotStanding), Knocked->GetConditionPin());
	OvLink(CanKnock->GetElsePin(), OvExec(Knocked));
	OvLink(Knocked->GetThenPin(), OvExec(Reset));
	OvLink(Piece, OvTarget(Reset));
	OvLink(OvPin(Cond, ConditionVar, EGPD_Output), OvPin(IsFallen, TEXT("A"), EGPD_Input));
	OvLink(OvOut(IsFallen), Fallen->GetConditionPin());
	OvLink(CanKnock->GetThenPin(), OvExec(Fallen));
	OvLink(Fallen->GetThenPin(), OvPin(MeshLoop, TEXT("Exec"), EGPD_Input));
	OvLink(OvPin(MeshesGet, GuardMeshesVar, EGPD_Output), OvPin(MeshLoop, TEXT("Array"), EGPD_Input));
	OvLink(OvLoopBody(MeshLoop), OvExec(Hide));
	OvLink(OvPin(MeshLoop, TEXT("Array Element"), EGPD_Output), OvTarget(Hide));
	OvLink(OvThen(Hide), OvExec(NoCollision));
	OvLink(OvPin(MeshLoop, TEXT("Array Element"), EGPD_Output), OvTarget(NoCollision));

	return OvFinishBlueprint(BP) ? BP : nullptr;
}

#undef LOCTEXT_NAMESPACE
