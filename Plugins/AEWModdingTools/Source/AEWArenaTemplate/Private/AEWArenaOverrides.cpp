#include "AEWArenaTools.h"
#include "AEWArenaTemplateSettings.h"
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
#include "K2Node_Message.h"
#include "K2Node_SetFieldsInStruct.h"
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

#define LOCTEXT_NAMESPACE "AEWArenaOverrides"

DEFINE_LOG_CATEGORY_STATIC(LogAEWArenaOverrides, Log, All);

namespace
{
	const TCHAR* StartMarkerName = TEXT("BP_StartMarker");
	const TCHAR* CameraOverrideName = TEXT("BP_CameraOverride");
	const FName StartNameVar(TEXT("StartName"));
	const FName CameraTableVar(TEXT("CameraTable"));
	const FName TryApplyEvent(TEXT("TryApply"));

	const UAEWArenaTemplateSettings* OvOverrideSettings()
	{
		return GetDefault<UAEWArenaTemplateSettings>();
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

UBlueprint* UAEWArenaTools::EnsureStartMarkerBlueprint(const FString& ArenaFolder, bool bRebuild)
{
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
			TEXT("Name of the player or referee start in ArenaMatch to move here, e.g. PlayerStart_1on1_00 or RefereeStart_Tag."));
	}

	UEdGraph* G = FBlueprintEditorUtils::FindEventGraph(BP);
	UK2Node_Event* Begin = OvBeginPlayNode(G, 0, 0);
	UK2Node_CallFunction* GetAll = OvCall(G, UGameplayStatics::StaticClass(), GET_FUNCTION_NAME_CHECKED(UGameplayStatics, GetAllActorsOfClass), 250, 0);
	OvSetObject(OvPin(GetAll, TEXT("ActorClass"), EGPD_Input), APlayerStart::StaticClass());
	UK2Node_MacroInstance* Loop = NewObject<UK2Node_MacroInstance>(G);
	Loop->SetMacroGraph(OvForEachMacro());
	OvPlace(G, Loop, 600, 0);
	UK2Node_CallFunction* ObjName = OvCall(G, UKismetSystemLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, GetObjectName), 600, 250);
	UK2Node_VariableGet* Wanted = OvGetSelfVar(G, StartNameVar, 600, 400);
	UK2Node_CallFunction* Equal = OvCall(G, UKismetStringLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetStringLibrary, EqualEqual_StrStr), 900, 300);
	UK2Node_IfThenElse* If = OvBranch(G, 1150, 0);
	UK2Node_CallFunction* Root = OvCall(G, AActor::StaticClass(), GET_FUNCTION_NAME_CHECKED(AActor, K2_GetRootComponent), 1150, 250);
	UK2Node_CallFunction* Mobility = OvCall(G, USceneComponent::StaticClass(), GET_FUNCTION_NAME_CHECKED(USceneComponent, SetMobility), 1400, 0);
	OvSetValue(OvPin(Mobility, TEXT("NewMobility"), EGPD_Input), TEXT("Movable"));
	UK2Node_CallFunction* MyLoc = OvCall(G, AActor::StaticClass(), GET_FUNCTION_NAME_CHECKED(AActor, K2_GetActorLocation), 1400, 250);
	UK2Node_CallFunction* MyRot = OvCall(G, AActor::StaticClass(), GET_FUNCTION_NAME_CHECKED(AActor, K2_GetActorRotation), 1400, 350);
	UK2Node_CallFunction* Move = OvCall(G, AActor::StaticClass(), GET_FUNCTION_NAME_CHECKED(AActor, K2_SetActorLocationAndRotation), 1700, 0);
	OvSetValue(OvPin(Move, TEXT("bTeleport"), EGPD_Input), TEXT("true"));
	if (!GetAll || !ObjName || !Equal || !Root || !Mobility || !MyLoc || !MyRot || !Move || !Loop->GetMacroGraph())
	{
		return nullptr;
	}

	OvLink(OvThen(Begin), OvExec(GetAll));
	OvLink(OvThen(GetAll), OvPin(Loop, TEXT("Exec"), EGPD_Input));
	OvLink(OvPin(GetAll, TEXT("OutActors"), EGPD_Output), OvPin(Loop, TEXT("Array"), EGPD_Input));
	OvLink(OvPin(Loop, TEXT("Loop Body"), EGPD_Output), OvExec(If));
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

	OvFinishBlueprint(BP);
	return BP;
}

UBlueprint* UAEWArenaTools::EnsureCameraOverrideBlueprint(const FString& ArenaFolder, bool bRebuild)
{
	UClass* CameraClass = OvFindGameplayCameraClass();
	if (!CameraClass)
	{
		UE_LOG(LogAEWArenaOverrides, Error, TEXT("class ELGameplayCamera not found - is the game's C++ module loaded?"));
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
			TEXT("Gameplay camera table (row struct GameplayCameraParam) this arena uses instead of DT_GameplayCameraParam."));
	}

	UEdGraph* G = FBlueprintEditorUtils::FindEventGraph(BP);
	UK2Node_Event* Begin = OvBeginPlayNode(G, 0, 0);
	UK2Node_CallFunction* StartTimer = OvCall(G, UKismetSystemLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, K2_SetTimer), 300, 0);
	OvSetValue(OvPin(StartTimer, TEXT("FunctionName"), EGPD_Input), TryApplyEvent.ToString());
	OvSetValue(OvPin(StartTimer, TEXT("Time"), EGPD_Input), TEXT("0.2"));
	OvSetValue(OvPin(StartTimer, TEXT("bLooping"), EGPD_Input), TEXT("true"));

	UK2Node_CustomEvent* Try = NewObject<UK2Node_CustomEvent>(G);
	Try->CustomFunctionName = TryApplyEvent;
	OvPlace(G, Try, 0, 300);
	UK2Node_CallFunction* Find = OvCall(G, UGameplayStatics::StaticClass(), GET_FUNCTION_NAME_CHECKED(UGameplayStatics, GetActorOfClass), 250, 300);
	OvSetObject(OvPin(Find, TEXT("ActorClass"), EGPD_Input), CameraClass);
	UK2Node_CallFunction* Valid = OvCall(G, UKismetSystemLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, IsValid), 550, 450);
	UK2Node_IfThenElse* If = OvBranch(G, 750, 300);
	UK2Node_VariableGet* Table = OvGetSelfVar(G, CameraTableVar, 750, 500);
	UK2Node_VariableSet* SetTable = NewObject<UK2Node_VariableSet>(G);
	SetTable->VariableReference.SetExternalMember(TEXT("GameplayCameraParamDataTable"), CameraClass);
	OvPlace(G, SetTable, 1000, 300);
	UK2Node_CallFunction* Refresh = OvCall(G, UKismetSystemLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, K2_SetTimer), 1300, 300);
	OvSetValue(OvPin(Refresh, TEXT("FunctionName"), EGPD_Input), TEXT("SetupParam"));
	OvSetValue(OvPin(Refresh, TEXT("Time"), EGPD_Input), TEXT("0.05"));
	UK2Node_CallFunction* Stop = OvCall(G, UKismetSystemLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, K2_ClearTimer), 1600, 300);
	OvSetValue(OvPin(Stop, TEXT("FunctionName"), EGPD_Input), TryApplyEvent.ToString());
	if (!StartTimer || !Find || !Valid || !Refresh || !Stop)
	{
		return nullptr;
	}

	OvLink(OvThen(Begin), OvExec(StartTimer));
	OvLink(OvThen(Try), OvExec(Find));
	OvLink(OvThen(Find), OvExec(If));
	OvLink(OvOut(Find), OvPin(Valid, TEXT("Object"), EGPD_Input));
	OvLink(OvOut(Valid), If->GetConditionPin());
	OvLink(If->GetThenPin(), OvExec(SetTable));
	OvLink(OvOut(Find), OvTarget(SetTable));
	OvLink(OvPin(Table, CameraTableVar, EGPD_Output), OvPin(SetTable, TEXT("GameplayCameraParamDataTable"), EGPD_Input));
	OvLink(OvThen(SetTable), OvExec(Refresh));
	OvLink(OvOut(Find), OvPin(Refresh, TEXT("Object"), EGPD_Input));
	OvLink(OvThen(Refresh), OvExec(Stop));

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

	void OvScaleCameraTable(UDataTable* Table, float Scale)
	{
		const UScriptStruct* RS = Table->GetRowStruct();
		if (!RS || Scale <= 1.f)
		{
			return;
		}
		const TCHAR* Fields[] = { TEXT("RadiusMax_Arena"), TEXT("RadiusMin_Arena"), TEXT("RadiusMax_Ring") };
		const float Factors[] = { Scale, 1.f + (Scale - 1.f) * 0.25f, 1.f + (Scale - 1.f) * 0.3f };
		for (const TPair<FName, uint8*>& Row : Table->GetRowMap())
		{
			for (int32 i = 0; i < 3; ++i)
			{
				if (FFloatProperty* P = FindFProperty<FFloatProperty>(RS, Fields[i]))
				{
					P->SetPropertyValue_InContainer(Row.Value, P->GetPropertyValue_InContainer(Row.Value) * Factors[i]);
				}
			}
		}
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
		Copy->Modify();
		OvScaleCameraTable(Copy, OvOverrideSettings()->CameraZoomOutScale);
		Copy->MarkPackageDirty();
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
	UBlueprint* BP = EnsureStartMarkerBlueprint(OvArenaFolderOf(World));
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
		OvReport(TEXT("No player or referee starts found. Open the arena's persistent map and make sure its ArenaMatch sublevel is loaded (Levels window)."));
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
	OvReport(FString::Printf(TEXT("Added %d start marker(s) to %s (%d were already there).\n\nMove the markers you want to change and delete the rest, then save the map. In game each marker moves its start in ArenaMatch to its own position."),
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
	UBlueprint* BP = EnsureCameraOverrideBlueprint(OvArenaFolderOf(World));
	UClass* OverrideClass = BP ? BP->GeneratedClass.Get() : nullptr;
	FObjectProperty* TableProp = OverrideClass ? FindFProperty<FObjectProperty>(OverrideClass, CameraTableVar) : nullptr;
	if (!TableProp)
	{
		OvReport(TEXT("Could not create BP_CameraOverride - see the Output Log."));
		return false;
	}

	const FString Folder = OvArenaFolderOf(World);
	bool bNewTable = false;
	UDataTable* Table = OvEnsureArenaCameraTable(Folder, FPackageName::GetShortName(World->GetOutermost()->GetName()), bNewTable);

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
			bNewTable ? *FString::Printf(TEXT(" (new copy of the game's table, zoomed out x%.2g)"), OvOverrideSettings()->CameraZoomOutScale) : TEXT(""));
		Message += TEXT("\nEdit its rows to tune the camera (RadiusMax_Arena = furthest zoom-out outside the ring), then save the map.");
	}
	else
	{
		Message += TEXT("\n\nNo camera table found or copied - set Camera Table on the actor by hand.");
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
		OvReport(FString::Printf(TEXT("Could not load the game's match rule interface or struct:\n%s %s\n%s %s"),
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
		? TEXT("Added a match rule override to the Level Blueprint.\n\nBeginPlay reads the game's match rule, saves it in OriginalMatchRule, passes it through 'Set members in FMatchRule' and sets it back on the game instance. EndPlay restores the saved rule. It runs first on BeginPlay, before your existing nodes.\n\nSelect the Set members node and tick the rule fields you want to change in its Details panel, set their values, compile and save the map.")
		: TEXT("The nodes were added but the Level Blueprint has compile errors - open it and check the Compiler Results."));
	return bOk;
}

#undef LOCTEXT_NAMESPACE
