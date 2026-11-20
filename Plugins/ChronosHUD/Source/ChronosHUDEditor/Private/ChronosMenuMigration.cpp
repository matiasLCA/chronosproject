#include "ChronosMenuSettings.h"
#include "ChronosStamina.h"

#include "EdGraph/EdGraph.h"
#include "EdGraphNode_Comment.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Character.h"
#include "HAL/IConsoleManager.h"
#include "InputAction.h"
#include "K2Node_CallFunction.h"
#include "K2Node_EnhancedInputAction.h"
#include "K2Node_Event.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_Self.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"

namespace
{
    UBlueprint* LoadBlueprint(const TCHAR* Path) { return LoadObject<UBlueprint>(nullptr, Path); }

    UEdGraph* Function(UBlueprint* Blueprint, FName Name)
    {
        for (UEdGraph* Graph : Blueprint->FunctionGraphs)
            if (Graph->GetFName() == Name) return Graph;
        return nullptr;
    }

    UK2Node_CallFunction* FindCall(UEdGraph* Graph, FName Name)
    {
        if (Graph)
            for (UEdGraphNode* Node : Graph->Nodes)
                if (auto* Call = Cast<UK2Node_CallFunction>(Node); Call && Call->FunctionReference.GetMemberName() == Name)
                    return Call;
        return nullptr;
    }

    template <typename T, typename Configure>
    T* NodeAt(UEdGraph& Graph, int32 X, int32 Y, Configure ConfigureNode)
    {
        FGraphNodeCreator<T> Creator(Graph);
        auto* Node = Creator.CreateNode();
        Node->NodePosX = X;
        Node->NodePosY = Y;
        ConfigureNode(*Node);
        Creator.Finalize();
        return Node;
    }

    UK2Node_CallFunction* AddCall(UEdGraph& Graph, int32 X, int32 Y, UClass* Class, FName Name)
    {
        return NodeAt<UK2Node_CallFunction>(Graph, X, Y, [=](auto& Node) { Node.SetFromFunction(Class->FindFunctionByName(Name)); });
    }

    void Wire(UEdGraphPin* From, UEdGraphPin* To)
    {
        ensure(From && To && GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(From, To));
    }

    void ConnectJohn(UEdGraph& Graph, UK2Node_CallFunction& Target)
    {
        auto* Self = NodeAt<UK2Node_Self>(Graph, Target.NodePosX - 145, Target.NodePosY + 100, [](auto&) {});
        Wire(Self->FindPinChecked(UEdGraphSchema_K2::PN_Self), Target.FindPinChecked(TEXT("John")));
    }

    bool InstallJump(UBlueprint* John)
    {
        const FName Cost(TEXT("JumpStaminaCost"));
        auto* JumpGraph = Function(John, TEXT("saltarSiPuede"));
        auto* Graph = FBlueprintEditorUtils::FindEventGraph(John);
        auto* Jump = FindCall(JumpGraph, TEXT("Jump"));
        if (!Jump || !Graph) return false;
        if (FindCall(JumpGraph, TEXT("CanStaminaJump"))) return true;

        UK2Node_EnhancedInputAction* JumpAction = nullptr;
        UK2Node_Event* Jumped = nullptr;
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (auto* Action = Cast<UK2Node_EnhancedInputAction>(Node);
                Action && Action->InputAction && Action->InputAction->GetFName() == TEXT("imc_jump"))
                JumpAction = Action;
            if (auto* Event = Cast<UK2Node_Event>(Node); Event && Event->EventReference.GetMemberName() == TEXT("OnJumped"))
                Jumped = Event;
        }
        if (!JumpAction || JumpAction->FindPinChecked(TEXT("Triggered"))->LinkedTo.Num() != 1 ||
            !JumpAction->FindPinChecked(TEXT("Started"))->LinkedTo.IsEmpty())
            return false;

        John->Modify();
        JumpGraph->Modify();
        Graph->Modify();
        FEdGraphPinType Float;
        Float.PinCategory = UEdGraphSchema_K2::PC_Real;
        Float.PinSubCategory = UEdGraphSchema_K2::PC_Float;
        FBlueprintEditorUtils::AddMemberVariable(John, Cost, Float, TEXT("30.0"));
        FBlueprintEditorUtils::SetBlueprintVariableCategory(John, Cost, nullptr, FText::FromString(TEXT("YukoDev|Stamina")), true);
        FBlueprintEditorUtils::SetBlueprintOnlyEditableFlag(John, Cost, false);

        const auto* SprintCheck = UChronosStamina::StaticClass();
        auto* CanJump = AddCall(*JumpGraph, Jump->NodePosX, Jump->NodePosY + 160, const_cast<UClass*>(SprintCheck), TEXT("CanStaminaJump"));
        ConnectJohn(*JumpGraph, *CanJump);
        auto* Branch = NodeAt<UK2Node_IfThenElse>(*JumpGraph, Jump->NodePosX + 240, Jump->NodePosY, [](auto&) {});
        const auto Incoming = Jump->GetExecPin()->LinkedTo;
        Jump->GetExecPin()->BreakAllPinLinks();
        for (auto* Source : Incoming) Wire(Source, Branch->GetExecPin());
        Wire(CanJump->GetReturnValuePin(), Branch->GetConditionPin());
        Wire(Branch->GetThenPin(), Jump->GetExecPin());
        Jump->NodePosX += 460;

        auto* Triggered = JumpAction->FindPinChecked(TEXT("Triggered"));
        auto* Destination = Triggered->LinkedTo[0];
        Triggered->BreakLinkTo(Destination);
        Wire(JumpAction->FindPinChecked(TEXT("Started")), Destination);
        if (!Jumped)
        {
            Jumped = NodeAt<UK2Node_Event>(*Graph, JumpAction->NodePosX, JumpAction->NodePosY - 280, [](UK2Node_Event& Event)
            {
                Event.EventReference.SetExternalMember(TEXT("OnJumped"), ACharacter::StaticClass());
                Event.bOverrideFunction = true;
            });
        }
        auto* Charge = AddCall(*Graph, Jumped->NodePosX + 245, Jumped->NodePosY, UChronosStamina::StaticClass(), TEXT("ChargeSuccessfulJump"));
        ConnectJohn(*Graph, *Charge);
        auto* Then = Jumped->FindPinChecked(UEdGraphSchema_K2::PN_Then);
        const auto Outgoing = Then->LinkedTo;
        Then->BreakAllPinLinks();
        Wire(Then, Charge->GetExecPin());
        for (auto* Output : Outgoing) Wire(Charge->GetThenPin(), Output);
        auto* Comment = NodeAt<UEdGraphNode_Comment>(*Graph, Jumped->NodePosX - 40, Jumped->NodePosY - 55, [](auto& Box)
        {
            Box.NodeComment = TEXT("HECHO POR YukoDev (STAMINA)");
            Box.NodeWidth = 650;
            Box.NodeHeight = 220;
        });
        Comment->AddNodeUnderComment(Jumped);
        Comment->AddNodeUnderComment(Charge);
        return true;
    }

    bool PreserveSettingsWhenSaving(UBlueprint* Instance)
    {
        auto* Graph = Function(Instance, TEXT("guardarOpciones"));
        if (FindCall(Graph, TEXT("LoadOptionsForWrite"))) return true;
        auto* Old = FindCall(Graph, TEXT("CreateSaveGameObject"));
        if (!Old) return false;
        auto* OldClass = Old->FindPin(TEXT("SaveGameClass"));
        if (!OldClass || !OldClass->DefaultObject) return false;

        Instance->Modify();
        Graph->Modify();
        auto* New = AddCall(*Graph, Old->NodePosX, Old->NodePosY, UChronosOptionsSave::StaticClass(), TEXT("LoadOptionsForWrite"));
        New->FindPinChecked(TEXT("SaveClass"))->DefaultObject = OldClass->DefaultObject;
        New->NodeComment = TEXT("YukoDev — HUD Y ACCESIBILIDAD");
        const auto Inputs = Old->GetExecPin()->LinkedTo;
        const auto Outputs = Old->GetThenPin()->LinkedTo;
        const auto Results = Old->GetReturnValuePin()->LinkedTo;
        Old->BreakAllNodeLinks();
        Old->DestroyNode();
        for (auto* Input : Inputs) Wire(Input, New->GetExecPin());
        for (auto* Output : Outputs) Wire(New->GetThenPin(), Output);
        for (auto* Result : Results) Wire(New->GetReturnValuePin(), Result);
        return true;
    }

    bool Compile(UBlueprint* Blueprint)
    {
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        return Blueprint->Status != BS_Error;
    }

    bool Save(UBlueprint* Blueprint)
    {
        auto* Package = Blueprint->GetOutermost();
        const FString File = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        return UPackage::SavePackage(Package, Blueprint, *File, Args);
    }

    void Apply()
    {
        auto* John = LoadBlueprint(TEXT("/Game/Characters/per_bp.per_bp"));
        auto* Options = LoadBlueprint(TEXT("/Game/UI/OptionsMenu.OptionsMenu"));
        auto* SaveGame = LoadBlueprint(TEXT("/Game/Guardado/SG_Opciones.SG_Opciones"));
        auto* Instance = LoadBlueprint(TEXT("/Game/Guardado/BP_GameInstance.BP_GameInstance"));
        if (!(John && Options && SaveGame && Instance) || !InstallJump(John) || !PreserveSettingsWhenSaving(Instance))
        {
            UE_LOG(LogTemp, Error, TEXT("CHRONOS_MENU_UPDATE: existing Blueprint graph differs. Nothing saved."));
            return;
        }
        Options->Modify();
        Options->ParentClass = UChronosOptionsMenu::StaticClass();
        SaveGame->Modify();
        SaveGame->ParentClass = UChronosOptionsSave::StaticClass();
        const TArray<UBlueprint*> Targets{SaveGame, Instance, Options, John};
        for (auto* Target : Targets)
            if (!Compile(Target))
            {
                UE_LOG(LogTemp, Error, TEXT("CHRONOS_MENU_UPDATE: failed compiling %s. Nothing saved."), *Target->GetPathName());
                return;
            }
        for (auto* Target : Targets)
        {
            if (!Save(Target))
            {
                UE_LOG(LogTemp, Error, TEXT("CHRONOS_MENU_UPDATE: failed saving %s."), *Target->GetPathName());
                return;
            }
            UE_LOG(LogTemp, Display, TEXT("CHRONOS_MENU_UPDATE: compiled and saved %s"), *Target->GetPathName());
        }
    }

    FAutoConsoleCommand Install(TEXT("Chronos.InstallMenuAndHUD"), TEXT("Install HUD, menu settings and jump stamina hooks."),
        FConsoleCommandDelegate::CreateStatic(&Apply));
}
