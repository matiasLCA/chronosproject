#include "ChronosStamina.h"

#include "EdGraph/EdGraph.h"
#include "EdGraphNode_Comment.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "HAL/IConsoleManager.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_Self.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "UObject/SavePackage.h"

namespace
{
    constexpr TCHAR JohnPath[] = TEXT("/Game/Characters/per_bp.per_bp");
    constexpr TCHAR Author[] = TEXT("HECHO POR YukoDev (STAMINA)");

    template<typename Node, typename Predicate>
    Node* Find(UEdGraph* Graph, Predicate Test)
    {
        if (!Graph) return nullptr;
        for (UEdGraphNode* Candidate : Graph->Nodes)
            if (Node* Typed = Cast<Node>(Candidate); Typed && Test(Typed)) return Typed;
        return nullptr;
    }

    template<typename Node>
    Node* Spawn(UEdGraph* Graph, FVector2D Position)
    {
        FGraphNodeCreator<Node> Creator(*Graph);
        Node* Value = Creator.CreateNode();
        Value->NodePosX = Position.X;
        Value->NodePosY = Position.Y;
        Creator.Finalize();
        return Value;
    }

    UK2Node_CallFunction* Call(UEdGraph* Graph, FName Name, FVector2D Position)
    {
        FGraphNodeCreator<UK2Node_CallFunction> Creator(*Graph);
        UK2Node_CallFunction* Value = Creator.CreateNode();
        Value->SetFromFunction(UChronosStamina::StaticClass()->FindFunctionByName(Name));
        Value->NodePosX = Position.X;
        Value->NodePosY = Position.Y;
        Creator.Finalize();
        return Value;
    }

    FVector2D Position(const UEdGraphNode* Node) { return FVector2D(Node->NodePosX, Node->NodePosY); }

    void Link(UEdGraphPin* From, UEdGraphPin* To)
    {
        check(From && To);
        const bool bConnected = GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(From, To);
        check(bConnected);
    }

    void ConnectJohn(UEdGraph* Graph, UK2Node_CallFunction* Function)
    {
        auto* Self = Spawn<UK2Node_Self>(Graph, Position(Function) - FVector2D(170, -110));
        Link(Self->FindPinChecked(UEdGraphSchema_K2::PN_Self), Function->FindPinChecked(TEXT("John")));
    }

    void InsertAfter(UEdGraphPin* Exec, UK2Node_CallFunction* Function)
    {
        const auto Links = Exec->LinkedTo;
        Exec->BreakAllPinLinks();
        Link(Exec, Function->GetExecPin());
        for (UEdGraphPin* Destination : Links) Link(Function->GetThenPin(), Destination);
    }

    UK2Node_CallFunction* FindCall(UEdGraph* Graph, FName Name)
    {
        return Find<UK2Node_CallFunction>(Graph, [Name](const UK2Node_CallFunction* Node)
        {
            return Node->FunctionReference.GetMemberName() == Name;
        });
    }

    void ReplaceAttack(UK2Node_CallFunction* Original, bool bKick)
    {
        UEdGraph* Graph = Original->GetGraph();
        auto* Replacement = Call(Graph, GET_FUNCTION_NAME_CHECKED(UChronosStamina, TryStaminaAttack), Position(Original));
        ConnectJohn(Graph, Replacement);
        Replacement->FindPinChecked(TEXT("bKick"))->DefaultValue = bKick ? TEXT("true") : TEXT("false");
        const auto Inputs = Original->GetExecPin()->LinkedTo;
        const auto Outputs = Original->GetThenPin()->LinkedTo;
        Original->BreakAllNodeLinks();
        Original->DestroyNode();
        for (UEdGraphPin* Input : Inputs) Link(Input, Replacement->GetExecPin());
        for (UEdGraphPin* Output : Outputs) Link(Replacement->GetThenPin(), Output);
    }

    void Apply()
    {
        UBlueprint* BP = LoadObject<UBlueprint>(nullptr, JohnPath);
        if (!BP) return;
        UEdGraph* Event = FBlueprintEditorUtils::FindEventGraph(BP);
        auto* Tick = Find<UK2Node_Event>(Event, [](const UK2Node_Event* Node)
        {
            return Node->EventReference.GetMemberName() == TEXT("ReceiveTick");
        });
        auto* Already = Find<UEdGraphNode_Comment>(Event, [](const UEdGraphNode_Comment* Node) { return Node->NodeComment == Author; });
        if (Already)
        {
            UE_LOG(LogTemp, Display, TEXT("CHRONOS_STAMINA: integration already installed."));
            return;
        }

        UEdGraph* Sprint = nullptr;
        UEdGraph* Punch = nullptr;
        UEdGraph* Kick = nullptr;
        for (UEdGraph* Graph : BP->FunctionGraphs)
        {
            if (Graph->GetFName() == TEXT("correrSiPuede")) Sprint = Graph;
            if (Graph->GetFName() == TEXT("atacar")) Punch = Graph;
            if (Graph->GetFName() == TEXT("patear")) Kick = Graph;
        }
        auto* Entry = Find<UK2Node_FunctionEntry>(Sprint, [](const auto*) { return true; });
        auto* TryPunch = FindCall(Punch, TEXT("TryAttack"));
        auto* TryKick = FindCall(Kick, TEXT("TryKick"));
        auto* SprintInput = Find<UEdGraphNode>(Event, [](const UEdGraphNode* Node)
        {
            return Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString().Contains(TEXT("imc_sprint"), ESearchCase::IgnoreCase);
        });
        auto* Completed = SprintInput ? SprintInput->FindPin(TEXT("Completed"), EGPD_Output) : nullptr;
        auto* Canceled = SprintInput ? SprintInput->FindPin(TEXT("Canceled"), EGPD_Output) : nullptr;
        UEdGraphPin* ReleaseExec = Completed && Completed->LinkedTo.Num() == 1 ? Completed->LinkedTo[0] : nullptr;
        auto* ReleaseNode = ReleaseExec ? ReleaseExec->GetOwningNode() : nullptr;
        auto* TickDelta = Tick ? Tick->FindPin(TEXT("DeltaSeconds"), EGPD_Output) : nullptr;
        auto* SprintThen = Entry ? Entry->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output) : nullptr;
        if (!TickDelta || !SprintThen || SprintThen->LinkedTo.Num() != 1 || !TryPunch || !TryKick ||
            !Canceled || !ReleaseExec || !Canceled->LinkedTo.Contains(ReleaseExec) ||
            !ReleaseNode->FindPin(UEdGraphSchema_K2::PN_Then))
        {
            UE_LOG(LogTemp, Error, TEXT("CHRONOS_STAMINA: topology differs (tick=%d entry=%d punch=%d kick=%d complete=%d cancel=%d)."),
                TickDelta != nullptr, SprintThen != nullptr, TryPunch != nullptr, TryKick != nullptr, Completed != nullptr, Canceled != nullptr);
            return;
        }

        BP->Modify();
        Event->Modify();
        struct FField { const TCHAR* Name; const TCHAR* Initial; bool bBool = false; };
        const FField Fields[] = {
            {TEXT("MaxStamina"), TEXT("100.0")}, {TEXT("CurrentStamina"), TEXT("100.0")},
            {TEXT("StaminaDrainRate"), TEXT("10.0")}, {TEXT("StaminaRegenRateIdle"), TEXT("5.0")},
            {TEXT("StaminaRegenRateWalking"), TEXT("3.0")}, {TEXT("StaminaRegenDelay"), TEXT("2.0")},
            {TEXT("StaminaSprintThreshold"), TEXT("20.0")}, {TEXT("PunchStaminaCost"), TEXT("10.0")},
            {TEXT("KickStaminaCost"), TEXT("18.0")}, {TEXT("LastStaminaUseTime"), TEXT("0.0")},
            {TEXT("bIsSprinting"), TEXT("false"), true}, {TEXT("bStaminaExhausted"), TEXT("false"), true},
            {TEXT("bSprintNeedsRelease"), TEXT("false"), true}
        };
        for (const FField& Field : Fields)
        {
            if (FBlueprintEditorUtils::FindNewVariableIndex(BP, Field.Name) != INDEX_NONE) continue;
            FEdGraphPinType Type;
            Type.PinCategory = Field.bBool ? UEdGraphSchema_K2::PC_Boolean : UEdGraphSchema_K2::PC_Real;
            if (!Field.bBool) Type.PinSubCategory = UEdGraphSchema_K2::PC_Double;
            FBlueprintEditorUtils::AddMemberVariable(BP, Field.Name, Type, Field.Initial);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(BP, Field.Name, nullptr, FText::FromString(TEXT("YukoDev|Stamina")), true);
        }

        const FVector2D TickPosition = Position(Tick) + FVector2D(350, -220);
        auto* Update = Call(Event, GET_FUNCTION_NAME_CHECKED(UChronosStamina, UpdateStamina), TickPosition);
        ConnectJohn(Event, Update);
        Link(TickDelta, Update->FindPinChecked(TEXT("DeltaSeconds")));
        InsertAfter(Tick->FindPinChecked(UEdGraphSchema_K2::PN_Then), Update);

        auto* Release = Call(Event, GET_FUNCTION_NAME_CHECKED(UChronosStamina, ReleaseSprint), Position(ReleaseNode) + FVector2D(300, 0));
        ConnectJohn(Event, Release);
        InsertAfter(ReleaseNode->FindPinChecked(UEdGraphSchema_K2::PN_Then), Release);

        UEdGraphPin* OriginalSprint = SprintThen->LinkedTo[0];
        SprintThen->BreakLinkTo(OriginalSprint);
        auto* Validate = Call(Sprint, GET_FUNCTION_NAME_CHECKED(UChronosStamina, BeginSprint), Position(Entry) + FVector2D(205, -196));
        ConnectJohn(Sprint, Validate);
        auto* Branch = Spawn<UK2Node_IfThenElse>(Sprint, Position(Validate) + FVector2D(245, 0));
        Link(SprintThen, Validate->GetExecPin());
        Link(Validate->GetThenPin(), Branch->GetExecPin());
        Link(Validate->GetReturnValuePin(), Branch->GetConditionPin());
        Link(Branch->GetThenPin(), OriginalSprint);

        ReplaceAttack(TryPunch, false);
        ReplaceAttack(TryKick, true);

        auto* Comment = Spawn<UEdGraphNode_Comment>(Event, TickPosition - FVector2D(193, 63));
        Comment->NodeComment = Author;
        Comment->NodeWidth = 490;
        Comment->NodeHeight = 232;
        Comment->CommentColor = FLinearColor(.23f, .37f, .55f);
        Comment->AddNodeUnderComment(Update);

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
        FKismetEditorUtilities::CompileBlueprint(BP);
        if (BP->Status == BS_Error)
        {
            UE_LOG(LogTemp, Error, TEXT("CHRONOS_STAMINA: Blueprint failed compile, asset not saved."));
            return;
        }
        UPackage* Package = BP->GetOutermost();
        const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        if (UPackage::SavePackage(Package, BP, *Filename, Args))
            UE_LOG(LogTemp, Display, TEXT("CHRONOS_STAMINA: compiled and saved %s."), JohnPath);
    }

    FAutoConsoleCommand InstallStamina(TEXT("Chronos.InstallStamina"), TEXT("Add validated stamina hooks to per_bp."),
        FConsoleCommandDelegate::CreateStatic(&Apply));
}

IMPLEMENT_MODULE(FDefaultModuleImpl, ChronosHUDEditor)
