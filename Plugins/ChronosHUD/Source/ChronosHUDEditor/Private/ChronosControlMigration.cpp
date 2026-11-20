#include "ChronosControls.h"
#include "ChronosStamina.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphNode_Comment.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "HAL/IConsoleManager.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "K2Node_CallFunction.h"
#include "K2Node_EnhancedInputAction.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_InputKey.h"
#include "K2Node_Self.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "PlayerMappableKeySettings.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "ChronosControlMigration"

namespace
{
    const TCHAR* Author = TEXT("YukoDev — CONFIGURACIÓN DE CONTROLES");

    struct FControl
    {
        FName Name;
        FText Label;
        FKey Key;
        const TCHAR* ActionPath;
        bool bReplaceLegacyEvent = false;
    };

    const FControl Controls[] = {
        {TEXT("Chronos.Forward"), LOCTEXT("Forward", "Avanzar"), EKeys::W, TEXT("/Game/Characters/input/movement/imc_adelante")},
        {TEXT("Chronos.Backward"), LOCTEXT("Backward", "Retroceder"), EKeys::S, TEXT("/Game/Characters/input/movement/imc_atras")},
        {TEXT("Chronos.Left"), LOCTEXT("Left", "Moverse a la izquierda"), EKeys::A, TEXT("/Game/Characters/input/movement/imc_izquierda")},
        {TEXT("Chronos.Right"), LOCTEXT("Right", "Moverse a la derecha"), EKeys::D, TEXT("/Game/Characters/input/movement/imc_derecha")},
        {TEXT("Chronos.Sprint"), LOCTEXT("Sprint", "Correr"), EKeys::LeftShift, TEXT("/Game/Characters/input/movement/imc_sprint")},
        {TEXT("Chronos.Crouch"), LOCTEXT("Crouch", "Agacharse"), EKeys::LeftControl, TEXT("/Game/Characters/input/movement/imc_crouch")},
        {TEXT("Chronos.Jump"), LOCTEXT("Jump", "Saltar"), EKeys::SpaceBar, TEXT("/Game/Characters/input/movement/imc_jump")},
        {TEXT("Chronos.Interact"), LOCTEXT("Interact", "Interactuar"), EKeys::E, TEXT("/Game/Characters/input/interaction/ia_interaction"), true},
        {TEXT("Chronos.Flashlight"), LOCTEXT("Flashlight", "Linterna"), EKeys::F, TEXT("/Game/Characters/input/interaction/IA_Flashlight"), true},
        {TEXT("Chronos.Incense"), LOCTEXT("Incense", "Incienso"), EKeys::C, TEXT("/Game/Characters/input/interaction/IA_UseIncense"), true},
        {TEXT("Chronos.Tasks"), LOCTEXT("Tasks", "Lista de tareas"), EKeys::G, TEXT("/Game/Characters/input/interaction/IA_TaskList"), true},
        {TEXT("Chronos.Drop"), LOCTEXT("Drop", "Soltar objeto"), EKeys::V, TEXT("/Game/Characters/input/interaction/IA_Drop"), true},
        {TEXT("Chronos.Attack"), LOCTEXT("Attack", "Atacar / barrer"), EKeys::LeftMouseButton, TEXT("/Game/Characters/input/interaction/IA_AttackSweep"), true},
        {TEXT("Chronos.Block"), LOCTEXT("Block", "Bloquear"), EKeys::RightMouseButton, TEXT("/Game/NPC_System/Combat/Input/IA_CombatBlock"), true},
        {TEXT("Chronos.Kick"), LOCTEXT("Kick", "Patada"), EKeys::R, TEXT("/Game/NPC_System/Combat/Input/IA_CombatKick"), true},
        {TEXT("Chronos.Pause"), LOCTEXT("Pause", "Pausa"), EKeys::P, TEXT("/Game/Characters/input/interaction/IA_Pause"), true},
        {TEXT("Chronos.Journal"), LOCTEXT("Journal", "Cuaderno"), EKeys::J, TEXT("/Game/Characters/input/interaction/IA_Journal")},
        {TEXT("Chronos.Map"), LOCTEXT("Map", "Mapa"), EKeys::M, TEXT("/Game/Characters/input/interaction/IA_Map")},
        {TEXT("Chronos.HUDMode"), LOCTEXT("HUDMode", "Cambiar modo del HUD"), EKeys::F8, TEXT("/Game/Characters/input/interaction/IA_HUDMode")},
    };

    bool Save(UObject* Asset)
    {
        UPackage* Package = Asset->GetOutermost();
        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
        return UPackage::SavePackage(Package, Asset, *Filename, Args);
    }

    void Wire(UEdGraphPin* From, UEdGraphPin* To)
    {
        ensure(From && To && GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(From, To));
    }

    UInputAction* GetAction(const FControl& Control, TArray<UObject*>& NewAssets)
    {
        const FString Name = FPackageName::GetLongPackageAssetName(Control.ActionPath);
        const FString ObjectPath = FString(Control.ActionPath) + TEXT(".") + Name;
        if (auto* Existing = LoadObject<UInputAction>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn)) return Existing;

        auto* Action = NewObject<UInputAction>(CreatePackage(Control.ActionPath), *Name, RF_Public | RF_Standalone | RF_Transactional);
        Action->ValueType = EInputActionValueType::Boolean;
        FAssetRegistryModule::AssetCreated(Action);
        Action->MarkPackageDirty();
        NewAssets.Add(Action);
        return Action;
    }

    void ReplaceEvents(UEdGraph& Graph, UInputAction* Action, const TArray<UK2Node_InputKey*>& Nodes)
    {
        if (Nodes.IsEmpty()) return;
        FGraphNodeCreator<UK2Node_EnhancedInputAction> Creator(Graph);
        auto* Event = Creator.CreateNode();
        Event->InputAction = Action;
        Event->NodePosX = Nodes[0]->NodePosX;
        Event->NodePosY = Nodes[0]->NodePosY;
        Event->NodeComment = Author;
        Creator.Finalize();

        TArray<UEdGraphPin*> Pressed;
        TArray<UEdGraphPin*> Released;
        for (auto* Node : Nodes)
        {
            Pressed.Append(Node->FindPinChecked(TEXT("Pressed"))->LinkedTo);
            Released.Append(Node->FindPinChecked(TEXT("Released"))->LinkedTo);
            Node->BreakAllNodeLinks();
            Node->DestroyNode();
        }

        auto* Started = Event->FindPinChecked(TEXT("Started"));
        if (Pressed.Num() == 1) Wire(Started, Pressed[0]);
        else if (Pressed.Num() > 1)
        {
            FGraphNodeCreator<UK2Node_ExecutionSequence> SequenceCreator(Graph);
            auto* Sequence = SequenceCreator.CreateNode();
            Sequence->NodePosX = Event->NodePosX + 180;
            Sequence->NodePosY = Event->NodePosY;
            SequenceCreator.Finalize();
            Wire(Started, Sequence->FindPinChecked(UEdGraphSchema_K2::PN_Execute));
            for (int32 Index = 0; Index < Pressed.Num(); ++Index)
            {
                if (Index > 1) Sequence->AddInputPin();
                Wire(Sequence->GetThenPinGivenIndex(Index), Pressed[Index]);
            }
        }
        for (auto* Release : Released)
        {
            Wire(Event->FindPinChecked(TEXT("Completed")), Release);
            Wire(Event->FindPinChecked(TEXT("Canceled")), Release);
        }
    }

    void InstallSprintStart(UBlueprint* John, UEdGraph& Graph, UK2Node_EnhancedInputAction& Sprint)
    {
        const FName HeldName(TEXT("bSprintInputHeld"));
        const bool bHasHeld = FBlueprintEditorUtils::FindNewVariableIndex(John, HeldName) != INDEX_NONE;
        if (!bHasHeld)
        {
            FEdGraphPinType Bool;
            Bool.PinCategory = UEdGraphSchema_K2::PC_Boolean;
            FBlueprintEditorUtils::AddMemberVariable(John, HeldName, Bool, TEXT("false"));
            FBlueprintEditorUtils::SetBlueprintVariableCategory(John, HeldName, nullptr, LOCTEXT("StaminaCategory", "YukoDev|Stamina"), true);
        }

        auto* Started = Sprint.FindPinChecked(TEXT("Started"));
        if (!Started->LinkedTo.IsEmpty()) return;
        FGraphNodeCreator<UK2Node_CallFunction> Creator(Graph);
        auto* Hold = Creator.CreateNode();
        Hold->SetFromFunction(UChronosStamina::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UChronosStamina, HoldSprintInput)));
        Hold->NodePosX = Sprint.NodePosX + 220;
        Hold->NodePosY = Sprint.NodePosY - 190;
        Creator.Finalize();
        FGraphNodeCreator<UK2Node_Self> SelfCreator(Graph);
        auto* Self = SelfCreator.CreateNode();
        Self->NodePosX = Hold->NodePosX - 135;
        Self->NodePosY = Hold->NodePosY + 60;
        SelfCreator.Finalize();
        Wire(Started, Hold->GetExecPin());
        Wire(Self->FindPinChecked(UEdGraphSchema_K2::PN_Self), Hold->FindPinChecked(TEXT("John")));

        FGraphNodeCreator<UEdGraphNode_Comment> CommentCreator(Graph);
        auto* Comment = CommentCreator.CreateNode();
        Comment->NodeComment = TEXT("HECHO POR YukoDev (STAMINA)");
        Comment->NodePosX = Self->NodePosX - 30;
        Comment->NodePosY = Hold->NodePosY - 45;
        Comment->NodeWidth = 500;
        Comment->NodeHeight = 176;
        CommentCreator.Finalize();
        Comment->AddNodeUnderComment(Self);
        Comment->AddNodeUnderComment(Hold);
    }

    void Apply()
    {
        auto* John = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Characters/per_bp.per_bp"));
        auto* Menu = LoadObject<UBlueprint>(nullptr, TEXT("/Game/UI/WBP_OpcionesControles.WBP_OpcionesControles"));
        auto* Context = const_cast<UInputMappingContext*>(ChronosControls::Context());
        auto* Graph = John ? FBlueprintEditorUtils::FindEventGraph(John) : nullptr;
        if (!John || !Menu || !Context || !Graph) return;

        TMap<FKey, TArray<UK2Node_InputKey*>> Legacy;
        UK2Node_EnhancedInputAction* Sprint = nullptr;
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (auto* RawKey = Cast<UK2Node_InputKey>(Node)) Legacy.FindOrAdd(RawKey->InputKey).Add(RawKey);
            if (auto* Enhanced = Cast<UK2Node_EnhancedInputAction>(Node);
                Enhanced && Enhanced->InputAction && Enhanced->InputAction->GetFName() == TEXT("imc_sprint"))
                Sprint = Enhanced;
        }
        if (!Sprint || FBlueprintEditorUtils::FindNewVariableIndex(John, TEXT("StaminaRegenDelay")) == INDEX_NONE)
        {
            UE_LOG(LogTemp, Error, TEXT("CHRONOS_CONTROLS: missing existing sprint/stamina hooks, migration stopped."));
            return;
        }
        for (const auto& Control : Controls)
        {
            if (Control.bReplaceLegacyEvent && !Legacy.Contains(Control.Key) &&
                !Context->GetMappings().ContainsByPredicate([&](const auto& Mapping) { return Mapping.GetMappingName() == Control.Name; }))
            {
                UE_LOG(LogTemp, Error, TEXT("CHRONOS_CONTROLS: missing source node for %s."), *Control.Name.ToString());
                return;
            }
            for (auto* RawKey : Legacy.FindRef(Control.Key))
            {
                if (RawKey->FindPinChecked(TEXT("Key"))->HasAnyConnections())
                {
                    UE_LOG(LogTemp, Error, TEXT("CHRONOS_CONTROLS: raw key value is consumed in %s."), *RawKey->GetName());
                    return;
                }
            }
        }

        John->Modify();
        Menu->Modify();
        Context->Modify();
        Graph->Modify();

        const auto* BehaviorProperty = FindFProperty<FEnumProperty>(FEnhancedActionKeyMapping::StaticStruct(), TEXT("SettingBehavior"));
        const auto* SettingsProperty = FindFProperty<FObjectProperty>(FEnhancedActionKeyMapping::StaticStruct(), TEXT("PlayerMappableKeySettings"));
        if (!BehaviorProperty || !SettingsProperty)
        {
            UE_LOG(LogTemp, Error, TEXT("CHRONOS_CONTROLS: the mapping properties are unavailable."));
            return;
        }

        TArray<UObject*> NewAssets;
        for (const auto& Control : Controls)
        {
            UInputAction* Action = GetAction(Control, NewAssets);
            const int32 Index = Context->GetMappings().IndexOfByPredicate([&](const auto& Mapping)
            {
                return Mapping.Action == Action && Mapping.Key == Control.Key;
            });
            auto& Mapping = Index == INDEX_NONE ? Context->MapKey(Action, Control.Key) : Context->GetMapping(Index);
            BehaviorProperty->GetUnderlyingProperty()->SetIntPropertyValue(BehaviorProperty->ContainerPtrToValuePtr<void>(&Mapping),
                static_cast<uint64>(EPlayerMappableKeySettingBehaviors::OverrideSettings));
            auto* Settings = Mapping.GetPlayerMappableKeySettings();
            if (!Settings)
            {
                Settings = NewObject<UPlayerMappableKeySettings>(Context, NAME_None, RF_Transactional);
                SettingsProperty->SetObjectPropertyValue_InContainer(&Mapping, Settings);
            }
            Settings->Name = Control.Name;
            Settings->DisplayName = Control.Label;
            if (Control.bReplaceLegacyEvent) ReplaceEvents(*Graph, Action, Legacy.FindRef(Control.Key));
        }

        InstallSprintStart(John, *Graph, *Sprint);
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (auto* Call = Cast<UK2Node_CallFunction>(Node);
                Call && Call->FunctionReference.GetMemberName() == TEXT("AddMappingContext"))
            {
                if (auto* Options = Call->FindPin(TEXT("Options"), EGPD_Input))
                    Options->DefaultValue.ReplaceInline(TEXT("bNotifyUserSettings=False"), TEXT("bNotifyUserSettings=True"));
            }
        }

        Menu->ParentClass = UChronosControls::StaticClass();
        for (UBlueprint* BP : {John, Menu})
        {
            FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
            FKismetEditorUtilities::CompileBlueprint(BP);
            if (BP->Status == BS_Error)
            {
                UE_LOG(LogTemp, Error, TEXT("CHRONOS_CONTROLS: %s failed to compile. No packages saved."), *BP->GetName());
                return;
            }
        }

        UObject* Defaults = John->GeneratedClass->GetDefaultObject();
        Defaults->Modify();
        const TPair<FName, double> Rates[] = {
            {TEXT("StaminaRegenRateIdle"), 5.}, {TEXT("StaminaRegenRateWalking"), 3.}, {TEXT("StaminaRegenDelay"), 2.}
        };
        for (const auto& Rate : Rates)
        {
            if (const auto* Property = FindFProperty<FNumericProperty>(Defaults->GetClass(), Rate.Key))
                Property->SetFloatingPointPropertyValue(Property->ContainerPtrToValuePtr<void>(Defaults), Rate.Value);
            const int32 Slot = FBlueprintEditorUtils::FindNewVariableIndex(John, Rate.Key);
            if (Slot != INDEX_NONE) John->NewVariables[Slot].DefaultValue = FString::SanitizeFloat(Rate.Value);
        }

        NewAssets.Append({Context, John, Menu});
        for (UObject* Asset : NewAssets)
        {
            if (!Save(Asset))
            {
                UE_LOG(LogTemp, Error, TEXT("CHRONOS_CONTROLS: failed to save %s."), *Asset->GetPathName());
                return;
            }
            UE_LOG(LogTemp, Display, TEXT("CHRONOS_CONTROLS: saved %s."), *Asset->GetPathName());
        }
        UE_LOG(LogTemp, Display, TEXT("CHRONOS_CONTROLS: migration finished."));
    }

    FAutoConsoleCommand Install(TEXT("Chronos.InstallControlSettings"),
        TEXT("Migrate Chronos controls to Enhanced Input and wire sprint input state."),
        FConsoleCommandDelegate::CreateStatic(&Apply));
}

#undef LOCTEXT_NAMESPACE
