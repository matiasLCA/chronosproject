#include "ChronosHUDLayer.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "Engine/Texture.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/RenderingCommon.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "Layout/Clipping.h"

namespace
{
    const FLinearColor Ivory = FLinearColor::FromSRGBColor(FColor(234, 227, 213));
    const FLinearColor Gold = FLinearColor::FromSRGBColor(FColor(220, 181, 76));
    const FLinearColor PinColor = FLinearColor::FromSRGBColor(FColor(105, 225, 224));
    const FLinearColor Dark(.017f, .016f, .016f, .94f);
    constexpr float MinimapRange = 13500.f;

    FVector2D AtlasUV(const FVector& World)
    {
        const FVector Offset = World - ChronosAtlas::Center;
        return FVector2D(.5 + Offset.Y / ChronosAtlas::Width, .5 - Offset.X / ChronosAtlas::Width);
    }

    FString DistanceText(const FVector& From, const FVector& To)
    {
        return FString::Printf(TEXT("%d m"), FMath::RoundToInt(FVector::Dist2D(From, To) / 100.0));
    }

    struct FHUDLayout
    {
        FVector2D Start = FVector2D::ZeroVector;
        FVector2D End = FVector2D::ZeroVector;
        FVector2D Size = FVector2D::ZeroVector;
        double Scale = 0.;
    };

    FHUDLayout LayoutFor(const UUserWidget* Widget, const FGeometry& Geometry)
    {
        FHUDLayout Layout;
        const FVector2D Bounds = Geometry.GetLocalSize();
        Layout.Scale = FMath::Min(Bounds.X / 1920., Bounds.Y / 1080.);
        if (Layout.Scale <= 0.) return Layout;
        FVector4 SafePadding;
        FVector4 Spill;
        FVector2D PaddingScale;
        UWidgetBlueprintLibrary::GetSafeZonePadding(Widget, SafePadding, PaddingScale, Spill);
        const double PixelScale = FMath::Max(1.e-3, static_cast<double>(Geometry.Scale));
        Layout.Start = FVector2D(SafePadding.X, SafePadding.Y) / PixelScale;
        Layout.End = Bounds - FVector2D(SafePadding.Z, SafePadding.W) / PixelScale;
        Layout.Size = (Layout.End - Layout.Start) / Layout.Scale;
        return Layout;
    }
}

UChronosHUDLayer::FMapView UChronosHUDLayer::MapView(FVector2D LayoutSize)
{
    const FVector2D Crop = ChronosAtlas::MapMax - ChronosAtlas::MapMin;
    const double Height = FMath::Max(1., FMath::Min(LayoutSize.Y - 220., 880.));
    const double Width = FMath::Max(1., FMath::Min(Height * Crop.X / Crop.Y, LayoutSize.X * .55));
    return { FVector2D((LayoutSize.X - Width - 370.) * .5, (LayoutSize.Y - Height) * .5 + 12.),
        FVector2D(Width, Height) };
}

FVector2D UChronosHUDLayer::UVAt(FVector2D Fraction) const
{
    return MapCenter + (ChronosAtlas::MapMax - ChronosAtlas::MapMin) / MapZoom * (Fraction - FVector2D(.5));
}

void UChronosHUDLayer::SetMapZoomBounds()
{
    const FVector2D Half = (ChronosAtlas::MapMax - ChronosAtlas::MapMin) * (.5 / MapZoom);
    const FVector2D Lo = ChronosAtlas::MapMin + Half;
    const FVector2D Hi = ChronosAtlas::MapMax - Half;
    MapCenter.X = FMath::Clamp(MapCenter.X, Lo.X, Hi.X);
    MapCenter.Y = FMath::Clamp(MapCenter.Y, Lo.Y, Hi.Y);
}

FReply UChronosHUDLayer::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
    if (!bMapOpen) return Super::NativeOnMouseButtonDown(Geometry, Event);
    const FKey Button = Event.GetEffectingButton();
    if (Button == EKeys::RightMouseButton)
    {
        Waypoint.Reset();
        WaypointLabel.Reset();
        UE_LOG(LogTemp, Display, TEXT("ChronosHUD: waypoint cleared."));
        return FReply::Handled();
    }
    if (Button != EKeys::LeftMouseButton) return FReply::Handled();
    const FHUDLayout Layout = LayoutFor(this, Geometry);
    if (Layout.Scale <= 0. || !bAtlasReady) return FReply::Handled();
    const FMapView View = MapView(Layout.Size);
    const FVector2D Local = (Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()) - Layout.Start) / Layout.Scale;
    const FVector2D Fraction = (Local - View.Position) / View.Size;
    if (Fraction.X < 0. || Fraction.X > 1. || Fraction.Y < 0. || Fraction.Y > 1.) return FReply::Handled();
    const TSharedPtr<SWidget> Slate = GetCachedWidget();
    if (!Slate.IsValid()) return FReply::Handled();
    PointerStart = Local;
    CenterAtPress = MapCenter;
    bMapPressed = true;
    bMapDragged = false;
    return FReply::Handled().CaptureMouse(Slate.ToSharedRef());
}

FReply UChronosHUDLayer::NativeOnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event)
{
    if (!bMapOpen || !bMapPressed) return Super::NativeOnMouseMove(Geometry, Event);
    const FHUDLayout Layout = LayoutFor(this, Geometry);
    if (Layout.Scale <= 0.) return FReply::Handled();
    const FVector2D Local = (Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()) - Layout.Start) / Layout.Scale;
    const FVector2D Drag = Local - PointerStart;
    if (Drag.SizeSquared() >= FMath::Square(6.)) bMapDragged = true;
    if (bMapDragged)
    {
        MapCenter = CenterAtPress - (ChronosAtlas::MapMax - ChronosAtlas::MapMin) / MapZoom
            * (Drag / MapView(Layout.Size).Size);
        SetMapZoomBounds();
    }
    return FReply::Handled();
}

FReply UChronosHUDLayer::NativeOnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event)
{
    if (Event.GetEffectingButton() != EKeys::LeftMouseButton || !bMapPressed)
        return Super::NativeOnMouseButtonUp(Geometry, Event);
    bMapPressed = false;
    const FHUDLayout Layout = LayoutFor(this, Geometry);
    if (bMapOpen && !bMapDragged && Layout.Scale > 0.)
    {
        const FMapView View = MapView(Layout.Size);
        const FVector2D Local = (Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()) - Layout.Start) / Layout.Scale;
        const FVector2D Fraction = (Local - View.Position) / View.Size;
        if ((Local - PointerStart).SizeSquared() < FMath::Square(6.)
            && Fraction.X >= 0. && Fraction.X <= 1. && Fraction.Y >= 0. && Fraction.Y <= 1.)
            PlaceMapPin(View, Fraction);
    }
    return FReply::Handled().ReleaseMouseCapture();
}

void UChronosHUDLayer::NativeOnMouseCaptureLost(const FCaptureLostEvent& Event)
{
    bMapPressed = false;
    bMapDragged = false;
    Super::NativeOnMouseCaptureLost(Event);
}

void UChronosHUDLayer::PlaceMapPin(const FMapView& View, FVector2D Fraction)
{
    const FVector2D UV = UVAt(Fraction);
    FVector Destination(ChronosAtlas::Center.X + (.5 - UV.Y) * ChronosAtlas::Width,
        ChronosAtlas::Center.Y + (UV.X - .5) * ChronosAtlas::Width, Location.Z - 88.);
    WaypointLabel = TEXT("Destino marcado");
    double SnapPixels = 19.;
    for (const FChronosPlace& Place : Places)
    {
        const FVector2D PlaceFraction = (AtlasUV(Place.Position) - UVAt(FVector2D::ZeroVector))
            / (UVAt(FVector2D(1.)) - UVAt(FVector2D::ZeroVector));
        const double Distance = ((PlaceFraction - Fraction) * View.Size).Size();
        if (Distance < SnapPixels)
        {
            SnapPixels = Distance;
            Destination = Place.Position;
            WaypointLabel = Place.Label;
        }
    }
    if (SnapPixels == 19. && GetWorld())
    {
        FHitResult Hit;
        const FVector Above(Destination.X, Destination.Y, 30000.);
        const FVector Below(Destination.X, Destination.Y, 18000.);
        FCollisionQueryParams Query(SCENE_QUERY_STAT(ChronosMap), true, GetOwningPlayerPawn());
        if (GetWorld()->LineTraceSingleByChannel(Hit, Above, Below, ECC_Visibility, Query))
            Destination.Z = Hit.ImpactPoint.Z;
    }
    Waypoint = Destination;
    UE_LOG(LogTemp, Display, TEXT("ChronosHUD: waypoint placed at %s (%s)."), *Destination.ToCompactString(), *WaypointLabel);
}

FReply UChronosHUDLayer::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    if (!bMapOpen) return Super::NativeOnKeyDown(Geometry, Event);
    const auto Key = Event.GetKey();
    if (Key == MapCloseKey || Key == PadMapCloseKey || Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right)
    {
        if (!Event.IsRepeat()) bMapOpen = false;
        return FReply::Handled();
    }
    if (Key == EKeys::Gamepad_RightShoulder || Key == EKeys::Gamepad_LeftShoulder)
    {
        MapZoom = FMath::Clamp(MapZoom * (Key == EKeys::Gamepad_RightShoulder ? 1.25 : .8), 1., 4.);
        SetMapZoomBounds();
        return FReply::Handled();
    }
    FVector2D Direction = FVector2D::ZeroVector;
    if (Key == EKeys::Gamepad_DPad_Left) Direction.X = -1;
    if (Key == EKeys::Gamepad_DPad_Right) Direction.X = 1;
    if (Key == EKeys::Gamepad_DPad_Up) Direction.Y = -1;
    if (Key == EKeys::Gamepad_DPad_Down) Direction.Y = 1;
    MapCenter += Direction * (ChronosAtlas::MapMax - ChronosAtlas::MapMin) / (40 * MapZoom);
    SetMapZoomBounds();
    return FReply::Handled();
}

FReply UChronosHUDLayer::NativeOnAnalogValueChanged(const FGeometry& Geometry, const FAnalogInputEvent& Event)
{
    if (!bMapOpen) return Super::NativeOnAnalogValueChanged(Geometry, Event);
    const FKey Key = Event.GetKey();
    if (FMath::Abs(Event.GetAnalogValue()) >= .32f && (Key == EKeys::Gamepad_LeftX || Key == EKeys::Gamepad_LeftY))
    {
        const float Move = Event.GetAnalogValue() / FMath::Max(1., MapZoom);
        const FVector2D Direction = Key == EKeys::Gamepad_LeftX ? FVector2D(Move, 0) : FVector2D(0, -Move);
        MapCenter += Direction * (ChronosAtlas::MapMax - ChronosAtlas::MapMin) * GetWorld()->GetDeltaSeconds();
        SetMapZoomBounds();
        return FReply::Handled();
    }
    return FReply::Unhandled();
}

FReply UChronosHUDLayer::NativeOnMouseWheel(const FGeometry& Geometry, const FPointerEvent& Event)
{
    if (!bMapOpen) return Super::NativeOnMouseWheel(Geometry, Event);
    const FHUDLayout Layout = LayoutFor(this, Geometry);
    if (Layout.Scale <= 0. || bMapPressed) return FReply::Handled();
    const FMapView View = MapView(Layout.Size);
    const FVector2D Local = (Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()) - Layout.Start) / Layout.Scale;
    const FVector2D Fraction = (Local - View.Position) / View.Size;
    if (Fraction.X < 0. || Fraction.X > 1. || Fraction.Y < 0. || Fraction.Y > 1.) return FReply::Handled();
    const FVector2D Anchor = UVAt(Fraction);
    MapZoom = FMath::Clamp(MapZoom * FMath::Pow(1.25, Event.GetWheelDelta()), 1., 4.);
    MapCenter += Anchor - UVAt(Fraction);
    SetMapZoomBounds();
    return FReply::Handled();
}

void UChronosHUDLayer::SetScales(const FChronosHUDScales& Value)
{
    if (Scales == Value) return;
    Scales = Value;
    InvalidateLayoutAndVolatility();
}

void UChronosHUDLayer::InitializeArt()
{
    auto Load = [this](const TCHAR* Key, const TCHAR* Relative)
    {
        const FString Path = FString(TEXT("/Game/TheChronosCurse_HUD_Assets/")) + Relative;
        UTexture* Texture = LoadObject<UTexture>(nullptr, *(Path + TEXT(".") + FPaths::GetCleanFilename(Path)), nullptr, LOAD_NoWarn);
        if (!Texture) return;
        Textures.Add(Key, Texture);
        auto& Brush = Brushes.Add(Key);
        Brush.DrawAs = ESlateBrushDrawType::Image;
        Brush.SetResourceObject(Texture);
    };

    Load(TEXT("Compass"), TEXT("Compass/T_UI_Compass_Frame"));
    Load(TEXT("Clock"), TEXT("Clock/T_UI_Clock_Frame_1024x300"));
    Load(TEXT("Health"), TEXT("Health/T_UI_Health_Frame_1024x256"));
    Load(TEXT("Green"), TEXT("Health/T_UI_Health_Fill_Green_1024x96"));
    Load(TEXT("Amber"), TEXT("Health/T_UI_Health_Fill_Amber_1024x96"));
    Load(TEXT("Red"), TEXT("Health/T_UI_Health_Fill_Red_1024x96"));
    Load(TEXT("StaminaFrame"), TEXT("Stamina/stamina2"));
    Load(TEXT("StaminaBlue"), TEXT("Stamina/blue"));
    Load(TEXT("StaminaSkyBlue"), TEXT("Stamina/skyblue"));
    Load(TEXT("StaminaWhite"), TEXT("Stamina/white"));
    for (const TCHAR* Fill : {TEXT("StaminaBlue"), TEXT("StaminaSkyBlue"), TEXT("StaminaWhite")})
    {
        if (FSlateBrush* Brush = Brushes.Find(Fill))
            Brush->SetUVRegion(FBox2f(FVector2f(0.f, .375f), FVector2f(1.f, .626f)));
    }
    Load(TEXT("Lamp"), TEXT("Flashlight/T_UI_Flashlight_Frame_1024x256"));
    Load(TEXT("BatteryWhite"), TEXT("Flashlight/T_UI_Battery_Segment_White_128"));
    Load(TEXT("BatteryAmber"), TEXT("Flashlight/T_UI_Battery_Segment_Amber_128"));
    Load(TEXT("BatteryRed"), TEXT("Flashlight/T_UI_Battery_Segment_Red_128"));
    Load(TEXT("Player"), TEXT("Objectives/T_UI_Player_Arrow_256"));
    Load(TEXT("Primary"), TEXT("Objectives/T_UI_Objective_Main_256"));
    Load(TEXT("Secondary"), TEXT("Objectives/T_UI_Objective_Secondary_256"));
    Load(TEXT("Done"), TEXT("Objectives/T_UI_Checkbox_Checked_192"));
    Load(TEXT("Todo"), TEXT("Objectives/T_UI_Checkbox_Empty_192"));
    Load(TEXT("Legend"), TEXT("Panels/T_UI_Panel_LegendBack_1024x768"));
    Load(TEXT("Quest"), TEXT("Panels/T_UI_Panel_QuestBack_1024x640"));
    Load(TEXT("Plaque"), TEXT("Panels/T_UI_Objective_Plaque_1024x256"));
    Load(TEXT("Key"), TEXT("Panels/T_UI_Control_Keycap_192"));
    const TCHAR* PlaceIcons[] = {TEXT("Home"), TEXT("Morgue"), TEXT("Church"), TEXT("Restroom"),
        TEXT("Graves"), TEXT("Playground"), TEXT("Flowers"), TEXT("Altar"), TEXT("Entrance"),
        TEXT("Parking"), TEXT("Crematorium")};
    for (const TCHAR* Icon : PlaceIcons)
        Load(Icon, *FString::Printf(TEXT("LocationIcons/T_UI_%s_256"), Icon));

    Serif = LoadObject<UFont>(nullptr, TEXT("/Game/assets/Cementerio/Tipografia/TrajanPro3SemiBold_Font.TrajanPro3SemiBold_Font"),
        nullptr, LOAD_NoWarn);
    if (Serif && Serif->FontCacheType != EFontCacheType::Runtime) Serif = nullptr;
}

void UChronosHUDLayer::SetAtlas(UTexture* Texture)
{
    if (!Texture)
    {
        Textures.Remove(TEXT("Atlas"));
        Brushes.Remove(TEXT("Atlas"));
        return;
    }
    Textures.Add(TEXT("Atlas"), Texture);
    FSlateBrush& Brush = Brushes.FindOrAdd(TEXT("Atlas"));
    Brush.DrawAs = ESlateBrushDrawType::Image;
    Brush.SetResourceObject(Texture);
}

int32 UChronosHUDLayer::NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling,
    FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bEnabled) const
{
    Layer = Super::NativePaint(Args, Geometry, Culling, Elements, Layer, Style, bEnabled) + 1;
    const FVector2D Size = Geometry.GetLocalSize();
    if (Size.X <= 0 || Size.Y <= 0 || bNarrativeHidden) return Layer;
    const FHUDLayout Layout = LayoutFor(this, Geometry);
    float Scale = Layout.Scale;
    if (Scale <= 0) return Layer;
    FVector2D Start = Layout.Start;
    const FVector2D LayoutSize = Layout.Size;
    const double W = LayoutSize.X;
    const double H = LayoutSize.Y;
    const bool bGuided = Mode == EChronosHUDMode::Guided;

    // YukoDev — HUD Y ACCESIBILIDAD
    // Preserve the original draw coordinates, including outlines and text, at scale 1.
    const auto ApplyScale = [&](float Factor, FVector2D OriginalOrigin, FVector2D NewOrigin)
    {
        Scale = Layout.Scale * Factor;
        Start = Layout.Start + (NewOrigin - OriginalOrigin * Factor) * Layout.Scale;
    };
    const auto ResetScale = [&] { ApplyScale(1.f, FVector2D::ZeroVector, FVector2D::ZeroVector); };

    auto Geom = [&](FVector2D Position, FVector2D Dimensions)
    {
        return Geometry.ToPaintGeometry(FVector2f(Dimensions * Scale), FSlateLayoutTransform(FVector2f(Start + Position * Scale)));
    };
    auto Box = [&](FVector2D P, FVector2D S, const FSlateBrush* Brush, const FLinearColor& Tint)
    {
        FSlateDrawElement::MakeBox(Elements, Layer, Geom(P, S), Brush, ESlateDrawEffect::None, Tint);
    };
    const FSlateBrush* White = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
    auto Art = [&](const TCHAR* Key, FVector2D P, FVector2D S, float Opacity = 1.f)
    {
        if (const FSlateBrush* Brush = Brushes.Find(Key))
            Box(P, S, Brush, FLinearColor(1, 1, 1, Opacity));
    };
    auto Panel = [&](const TCHAR* Key, FVector2D P, FVector2D S, float Opacity = 1.f)
    {
        if (Brushes.Contains(Key)) Art(Key, P, S, Opacity);
        else Box(P, S, White, FLinearColor(Dark.R, Dark.G, Dark.B, Opacity * Dark.A));
    };
    auto Text = [&](const FString& Value, FVector2D P, float Height, const FLinearColor& Color, float Align = 0.f)
    {
        if (Value.IsEmpty()) return;
        const int32 PointSize = FMath::Max(1, FMath::RoundToInt(Height * Scale));
        FSlateFontInfo Font = Serif ? FSlateFontInfo(Serif.Get(), PointSize) : FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), PointSize);
        if (Align > 0)
            P.X -= FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Value, Font).X * Align / Scale;
        const FVector2D Bounds(1000, Height * 1.6);
        FSlateDrawElement::MakeText(Elements, Layer, Geom(P + FVector2D(1.4, 1.4), Bounds), Value, Font,
            ESlateDrawEffect::None, FLinearColor(0, 0, 0, Color.A * .95f));
        FSlateDrawElement::MakeText(Elements, Layer, Geom(P, Bounds), Value, Font, ESlateDrawEffect::None, Color);
    };
    auto Line = [&](FVector2D From, FVector2D To, const FLinearColor& Tint, float Thickness)
    {
        TArray<FVector2f> Points{FVector2f(From * Scale), FVector2f(To * Scale)};
        FSlateDrawElement::MakeLines(Elements, Layer, Geom(FVector2D::ZeroVector, LayoutSize), MoveTemp(Points),
            ESlateDrawEffect::None, Tint, true, FMath::Max(1.f, Thickness * Scale));
    };
    auto Opacity = [](FLinearColor Color, float Alpha)
    {
        Color.A *= Alpha;
        return Color;
    };
    auto Icon = [&](const TCHAR* Key, FVector2D Center, float Diameter)
    {
        Art(Key, Center - FVector2D(Diameter * .5), FVector2D(Diameter));
    };
    auto Pin = [&](FVector2D Center, float Diameter)
    {
        const double R = Diameter * .29;
        const FVector2D Top = Center - FVector2D(0, R);
        const FVector2D Right = Center + FVector2D(R, 0);
        const FVector2D Tip = Center + FVector2D(0, R * 1.45);
        const FVector2D Left = Center - FVector2D(R, 0);
        for (const FLinearColor& Color : {FLinearColor(.008f, .020f, .023f), PinColor})
        {
            const float Width = Color == PinColor ? 2.1f : 4.8f;
            Line(Top, Right, Color, Width);
            Line(Right, Tip, Color, Width);
            Line(Tip, Left, Color, Width);
            Line(Left, Top, Color, Width);
        }
        Box(Center - FVector2D(2), FVector2D(4), White, PinColor);
    };
    auto DrawPlayer = [&](FVector2D Center, float Diameter)
    {
        if (const FSlateBrush* Brush = Brushes.Find(TEXT("Player")))
            FSlateDrawElement::MakeRotatedBox(Elements, Layer,
                Geom(Center - FVector2D(Diameter * .5), FVector2D(Diameter)), Brush,
                ESlateDrawEffect::None, FMath::DegreesToRadians(Heading));
    };
    const FChronosTask* Primary = Tasks.FindByPredicate([](const FChronosTask& Task)
    {
        return !Task.bCompleted && Task.Position.IsSet();
    });
    auto TaskName = [&](const FChronosTask& Task) -> FString
    {
        return Places.IsValidIndex(Task.PlaceIndex) ? Places[Task.PlaceIndex].Label : Task.Description;
    };
    auto DrawTasks = [&](FVector2D Origin, FVector2D Dimensions, const TCHAR* Title, bool bJournal)
    {
        Panel(TEXT("Quest"), Origin, Dimensions);
        Icon(TEXT("Primary"), Origin + FVector2D(31, 39), 27.f);
        Text(Title, Origin + FVector2D(52, 23), 23.f, Gold);
        const float RowHeight = bJournal ? 38.f : 30.f;
        const int32 Rows = FMath::Max(1, FMath::FloorToInt((Dimensions.Y - 80.) / RowHeight));
        const int32 Columns = bJournal ? FMath::Min(2, FMath::Max(1, FMath::DivideAndRoundUp(Tasks.Num(), Rows))) : 1;
        const double ColumnWidth = (Dimensions.X - 48.) / Columns;
        if (Tasks.IsEmpty()) Text(TEXT("No hay tareas asignadas."), Origin + FVector2D(23, 72), 16.f, Ivory);
        for (int32 Index = 0; Index < FMath::Min(Tasks.Num(), Rows * Columns); ++Index)
        {
            const FChronosTask& Task = Tasks[Index];
            const FVector2D Row = Origin + FVector2D(24 + (Index / Rows) * ColumnWidth, 74 + (Index % Rows) * RowHeight);
            Art(Task.bCompleted ? TEXT("Done") : TEXT("Todo"), Row, FVector2D(19, 19));
            const FString Progress = Task.ProgressText.IsEmpty() ? (Task.bCompleted ? TEXT("1/1") : TEXT("0/1")) : Task.ProgressText;
            const int32 MaxChars = bJournal ? 57 : 36;
            const FString Description = Task.Description.Len() <= MaxChars ? Task.Description : Task.Description.Left(MaxChars - 1) + TEXT("…");
            Text(Description, Row + FVector2D(27, 0), bJournal ? 17.f : 15.f, Task.bCompleted ? Opacity(Ivory, .55f) : Ivory);
            Text(Progress, Row + FVector2D(ColumnWidth - 3, 0), 15.f, Task.bCompleted ? Opacity(Ivory, .55f) : Gold, 1.f);
        }
    };

    auto DrawMap = [&](FVector2D Center, float Diameter, const FMapView* Full)
    {
        const bool bFull = Full != nullptr;
        const float Radius = Diameter * .5f;
        const FVector2D Top = Center - FVector2D(Radius);
        const FVector2D MidUV = bFull ? FVector2D(.5) : AtlasUV(Location);
        const double UVRadius = bFull ? .5 : MinimapRange / ChronosAtlas::Width;
        const auto Position = [&](const FVector& Point)
        {
            if (Full)
                return Full->Position + (AtlasUV(Point) - UVAt(FVector2D::ZeroVector))
                    / (UVAt(FVector2D(1.)) - UVAt(FVector2D::ZeroVector)) * Full->Size;
            return Center + (AtlasUV(Point) - MidUV) * (Radius / UVRadius);
        };
        const FSlateBrush* MapTexture = Brushes.Find(TEXT("Atlas"));
        if (bFull)
        {
            Box(Full->Position, Full->Size, White, Dark);
            if (MapTexture)
            {
                const FSlateRenderTransform Transform = Geom(Full->Position, Full->Size).GetAccumulatedRenderTransform();
                TArray<FSlateVertex> Verts;
                for (const FVector2D Corner : {FVector2D(0, 0), FVector2D(1, 0), FVector2D(1, 1), FVector2D(0, 1)})
                    Verts.Add(FSlateVertex::Make(Transform, FVector2f(Corner * Full->Size * Scale),
                        FVector2f(UVAt(Corner)), FColor(213, 213, 206)));
                const TArray<SlateIndex> Indices{0, 1, 2, 0, 2, 3};
                FSlateDrawElement::MakeCustomVerts(Elements, Layer + 1, FSlateApplication::Get().GetRenderer()->GetResourceHandle(*MapTexture),
                    Verts, Indices, nullptr, 0, 0, ESlateDrawEffect::IgnoreTextureAlpha);
            }
            ++Layer;
            Elements.PushClip(FSlateClippingZone(Geom(Full->Position, Full->Size)));
        }
        else
        {
            constexpr int32 Sides = 72;
            const FSlateRenderTransform Transform = Geom(Top, FVector2D(Diameter)).GetAccumulatedRenderTransform();
            TArray<FSlateVertex> Verts;
            TArray<SlateIndex> Indices;
            Verts.Reserve(Sides + 1);
            Indices.Reserve(Sides * 3);
            const FColor Tint = MapTexture ? FColor(151, 154, 148) : FColor(27, 29, 30, 240);
            Verts.Add(FSlateVertex::Make(Transform, FVector2f(Radius * Scale), FVector2f(MidUV), Tint));
            for (int32 Index = 0; Index < Sides; ++Index)
            {
                const float Angle = UE_TWO_PI * Index / Sides;
                const FVector2f Direction(FMath::Cos(Angle), FMath::Sin(Angle));
                const FVector2f UV = FVector2f(MidUV) + Direction * UVRadius;
                Verts.Add(FSlateVertex::Make(Transform, (FVector2f(1) + Direction) * (Radius * Scale),
                    FVector2f(FMath::Clamp(UV.X, 0.f, 1.f), FMath::Clamp(UV.Y, 0.f, 1.f)), Tint));
                Indices.Add(0);
                Indices.Add(Index + 1);
                Indices.Add((Index + 1) % Sides + 1);
            }
            const FSlateBrush* Brush = MapTexture ? MapTexture : White;
            FSlateDrawElement::MakeCustomVerts(Elements, Layer, FSlateApplication::Get().GetRenderer()->GetResourceHandle(*Brush),
                Verts, Indices, nullptr, 0, 0, MapTexture ? ESlateDrawEffect::IgnoreTextureAlpha : ESlateDrawEffect::None);
        }
        ++Layer;

        auto Fits = [&](const FVector2D& Point, double Margin)
        {
            const FVector2D Relative = Point - Center;
            return bFull ? Point.X >= Full->Position.X + Margin && Point.Y >= Full->Position.Y + Margin
                        && Point.X <= Full->Position.X + Full->Size.X - Margin && Point.Y <= Full->Position.Y + Full->Size.Y - Margin
                         : Relative.SizeSquared() <= FMath::Square(Radius - Margin);
        };
        for (int32 Index = 1; Index < Route.Num(); ++Index)
        {
            const FVector2D From = Position(Route[Index - 1]);
            const FVector2D To = Position(Route[Index]);
            const double Length = FVector2D::Distance(From, To);
            if (Length < 1.) continue;
            for (double Offset = 0; Offset < Length; Offset += 10.)
            {
                const FVector2D A = FMath::Lerp(From, To, Offset / Length);
                const FVector2D B = FMath::Lerp(From, To, FMath::Min(Offset + 5., Length) / Length);
                if (Fits(A, 5.) && Fits(B, 5.)) Line(A, B, Waypoint.IsSet() ? PinColor : Gold, 1.8f);
            }
        }
        ++Layer;
        for (const FChronosPlace& Place : Places)
        {
            const FVector2D Marker = Position(Place.Position);
            if (!Fits(Marker, bFull ? 0. : 16.)) continue;
            Icon(*Place.Icon, Marker, bFull ? 33.f : 25.f);
        }
        for (const FChronosTask& Task : Tasks)
        {
            if (Task.bCompleted || !Task.Position.IsSet()) continue;
            FVector2D Marker = Position(Task.Position.GetValue());
            if (!Fits(Marker, 18.))
            {
                if (bFull || &Task != Primary || Waypoint.IsSet()) continue;
                Marker = Center + (Marker - Center).GetSafeNormal() * (Radius - 25.f);
            }
            Icon(&Task == Primary ? TEXT("Primary") : TEXT("Secondary"), Marker, &Task == Primary ? 34.f : 23.f);
            if (&Task == Primary && !Waypoint.IsSet() && !bFull)
                Text(DistanceText(Location, Task.Position.GetValue()), Marker + FVector2D(0, 15), 14.f, Gold, .5f);
        }
        const FVector2D PlayerMarker = Position(Location);
        if (Fits(PlayerMarker, 0.)) DrawPlayer(PlayerMarker, 34.f);
        if (Waypoint.IsSet())
        {
            FVector2D Marker = Position(Waypoint.GetValue());
            if (!bFull && !Fits(Marker, 31.))
                Marker = Center + (Marker - Center).GetSafeNormal() * (Radius - 32.f);
            if (!bFull || Fits(Marker, 14.))
            {
                ++Layer;
                Pin(Marker, 40.f);
                if (!bFull)
                {
                    const double LabelOffset = Marker.Y > Center.Y + Radius * .55 ? -30. : 16.;
                    Text(DistanceText(Location, Waypoint.GetValue()), Marker + FVector2D(0, LabelOffset), 14.f, PinColor, .5f);
                }
            }
        }
        if (Full) Elements.PopClip();
    };

    if (bMapOpen)
    {
        Box(-Start / Scale, Size / Scale, White, FLinearColor(0, 0, 0, .91f));
        ++Layer;
        if (bMapOpen)
        {
            const FMapView View = MapView(LayoutSize);
            const FVector2D Sidebar(View.Position.X + View.Size.X + 32., View.Position.Y);
            const FLinearColor Stroke = Opacity(Gold, .42f);
            Box(View.Position - FVector2D(12), View.Size + FVector2D(24), White, FLinearColor(.023f, .022f, .021f, .97f));
            Box(View.Position - FVector2D(2), View.Size + FVector2D(4), White, Stroke);
            Box(Sidebar, FVector2D(338, View.Size.Y + 12.), White, FLinearColor(.022f, .022f, .023f, .98f));
            ++Layer;
            DrawMap(View.Position + View.Size * .5, 0, &View);
            ++Layer;
            Text(TEXT("Mapa del cementerio"), View.Position + FVector2D(0, -64), 32.f, Ivory);
            Text(TEXT("N"), View.Position + FVector2D(15, 12), 21.f, Ivory);
            Line(View.Position + FVector2D(23, 43), View.Position + FVector2D(23, 64), Ivory, 1.5f);
            Text(TEXT("Destino"), Sidebar + FVector2D(25, 28), 22.f, Gold);
            Line(Sidebar + FVector2D(25, 65), Sidebar + FVector2D(310, 65), Stroke, 1.f);
            if (Waypoint.IsSet())
            {
                Pin(Sidebar + FVector2D(40, 101), 34.f);
                Text(WaypointLabel, Sidebar + FVector2D(65, 83), 19.f, Ivory);
                Text(DistanceText(Location, Waypoint.GetValue()), Sidebar + FVector2D(65, 112), 17.f, PinColor);
            }
            else
                Text(TEXT("Elige un punto en el mapa"), Sidebar + FVector2D(25, 88), 15.f, Ivory);

            Text(TEXT("Leyenda"), Sidebar + FVector2D(25, 189), 22.f, Gold);
            Line(Sidebar + FVector2D(25, 226), Sidebar + FVector2D(310, 226), Stroke, 1.f);
            DrawPlayer(Sidebar + FVector2D(40, 260), 23.f);
            Icon(TEXT("Primary"), Sidebar + FVector2D(40, 302), 28.f);
            Pin(Sidebar + FVector2D(40, 344), 28.f);
            Text(TEXT("Tu posición"), Sidebar + FVector2D(66, 248), 16.f, Ivory);
            Text(TEXT("Objetivo de tarea"), Sidebar + FVector2D(66, 290), 16.f, Ivory);
            Text(TEXT("Pin personal"), Sidebar + FVector2D(66, 332), 16.f, Ivory);
            Text(TEXT("Lugares"), Sidebar + FVector2D(25, 376), 22.f, Gold);
            Line(Sidebar + FVector2D(25, 409), Sidebar + FVector2D(310, 409), Stroke, 1.f);
            TSet<FString> Listed;
            double SiteY = 425.;
            for (const FChronosPlace& Place : Places)
            {
                if (Listed.Contains(Place.Label)) continue;
                Listed.Add(Place.Label);
                Icon(*Place.Icon, Sidebar + FVector2D(38, SiteY + 9), 22.f);
                Text(Place.Label, Sidebar + FVector2D(60, SiteY), 14.f, Ivory);
                SiteY += 24.;
            }
            const FVector2D Footer = Sidebar + FVector2D(25, View.Size.Y - 156.);
            Line(Footer - FVector2D(0, 19), Footer + FVector2D(285, -19), Stroke, 1.f);
            Text(TEXT("Clic izq.    Marcar destino"), Footer, 14.f, Ivory);
            Text(TEXT("Arrastrar   Mover mapa"), Footer + FVector2D(0, 24), 14.f, Ivory);
            Text(TEXT("Clic der.    Quitar pin"), Footer + FVector2D(0, 48), 14.f, Ivory);
            Text(TEXT("Rueda      Acercar / alejar"), Footer + FVector2D(0, 72), 14.f, Ivory);
            if (MapKeyText.Len()) Text(MapKeyText + TEXT("             Cerrar"), Footer + FVector2D(0, 96), 14.f, Ivory);
            if (!bAtlasReady) Text(TEXT("Cartografía en preparación"), View.Position + View.Size * .5, 20.f, Ivory, .5f);
            return Layer;
        }
    }

    if (Mode != EChronosHUDMode::Immersive)
    {
        const FVector2D MapOrigin(25, 19);
        const float MapScale = Scales.Map / 100.f;
        ApplyScale(MapScale, MapOrigin, MapOrigin);
        const FVector2D Center = MapOrigin + FVector2D(143, 142);
        DrawMap(Center, 252.f, nullptr);
        ++Layer;
        Art(TEXT("Compass"), MapOrigin, FVector2D(280));
        Text(TEXT("N"), Center + FVector2D(0, -129), 17.f, Ivory, .5f);
        Text(TEXT("S"), Center + FVector2D(0, 109), 17.f, Ivory, .5f);
        Text(TEXT("O"), Center + FVector2D(-119, -11), 17.f, Ivory, .5f);
        Text(TEXT("E"), Center + FVector2D(117, -11), 17.f, Ivory, .5f);


        ResetScale();
        if (bGuided)
        {
            const FVector2D Origin(302 + FMath::Max(0.f, 280.f * (MapScale - 1.f)), 40);
            Panel(TEXT("Legend"), Origin, FVector2D(228, 160));
            Icon(TEXT("Player"), Origin + FVector2D(26, 32), 20.f);
            Icon(TEXT("Primary"), Origin + FVector2D(26, 62), 24.f);
            Icon(TEXT("Secondary"), Origin + FVector2D(26, 92), 22.f);
            for (int32 Offset = 0; Offset < 18; Offset += 6)
                Line(Origin + FVector2D(18 + Offset, 122), Origin + FVector2D(21 + Offset, 122), Gold, 1.6f);
            Text(TEXT("Tu posición"), Origin + FVector2D(43, 21), 16.f, Ivory);
            Text(TEXT("Objetivo principal"), Origin + FVector2D(43, 51), 16.f, Ivory);
            Text(TEXT("Objetivo secundario"), Origin + FVector2D(43, 81), 16.f, Ivory);
            Text(TEXT("Ruta de misión"), Origin + FVector2D(43, 111), 16.f, Ivory);
        }
        const FVector2D ClockOrigin(W - 358, 17);

        const float CalendarScale = Scales.Calendar / 100.f;
        const float CalendarExtra = FMath::Max(0.f, 104.f * (CalendarScale - 1.f));
        if (!Clock.IsEmpty())
        {
            ApplyScale(CalendarScale, ClockOrigin + FVector2D(335, 0), ClockOrigin + FVector2D(335, 0));
            Panel(TEXT("Clock"), ClockOrigin, FVector2D(335, 104));
            Text(Clock, ClockOrigin + FVector2D(306, 18), 23.f, Ivory, 1.f);
            Text(Day, ClockOrigin + FVector2D(306, 59), 21.f, Ivory, 1.f);
            ResetScale();
        }
        if (bGuided && !Tasks.IsEmpty())
            DrawTasks(FVector2D(W - 464, 135 + CalendarExtra), FVector2D(440, 265), TEXT("Tareas del día"), false);
        if (Waypoint.IsSet() || (Primary && !bGuided))
        {
            const bool bPin = Waypoint.IsSet();
            const FVector2D Objective(W - 399, (bGuided && !Tasks.IsEmpty() ? 411 : 131) + CalendarExtra);
            Box(Objective, FVector2D(376, 53), White, FLinearColor(.017f, .016f, .015f, .91f));
            Line(Objective, Objective + FVector2D(376, 0), Opacity(bPin ? PinColor : Gold, .48f), 1.f);
            if (bPin) Pin(Objective + FVector2D(23, 26), 33.f);
            else Icon(TEXT("Primary"), Objective + FVector2D(23, 26), 29.f);
            const FString Label = bPin ? WaypointLabel : TaskName(*Primary);
            const int32 PointSize = FMath::Max(1, FMath::RoundToInt(16.f * Scale));
            const FSlateFontInfo Font = Serif ? FSlateFontInfo(Serif.Get(), PointSize) : FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), PointSize);
            const auto Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
            FString Visible = Label;
            while (Visible.Len() > 2 && Measure->Measure(Visible, Font).X > 237.f * Scale)
                Visible = Visible.LeftChop(Visible.EndsWith(TEXT("…")) ? 2 : 1) + TEXT("…");
            Text(Visible, Objective + FVector2D(45, 13), 16.f, Ivory);
            const FVector& Target = bPin ? Waypoint.GetValue() : Primary->Position.GetValue();
            Text(DistanceText(Location, Target), Objective + FVector2D(360, 15), 15.f, bPin ? PinColor : Gold, 1.f);
        }
    }

    if (WorldMarker.IsSet() && (Waypoint.IsSet() || (bGuided && Primary)))
    {
        const FVector2D Marker = (WorldMarker.GetValue() * Size - Start) / Scale;
        const bool bPin = Waypoint.IsSet();
        if (bPin) Pin(Marker, 43.f);
        else Icon(TEXT("Primary"), Marker, 42.f);
        Text((bPin ? WaypointLabel : TaskName(*Primary)).Left(36), Marker + FVector2D(0, 22), 19.f, Ivory, .5f);
        const FVector& Target = bPin ? Waypoint.GetValue() : Primary->Position.GetValue();
        Text(DistanceText(Location, Target), Marker + FVector2D(0, 45), 16.f, bPin ? PinColor : Gold, .5f);
    }

    const double Bottom = H - 130;
    constexpr double FrameHeight = 113.;
    constexpr double HealthTop = 113. * 7. / 64.; // first visible pixels in the frame at its original draw size
    constexpr double StaminaBottom = 113. * 65. / 85.;
    constexpr double BarGap = 10.;
    const double LifeScale = Scales.Life / 100.;
    const double StaminaScale = Scales.Stamina / 100.;
    const double LampScale = Scales.Lamp / 100.;
    const double LifeTop = Bottom + FrameHeight * (1. - LifeScale);
    const double StaminaTop = LifeTop + HealthTop * LifeScale - BarGap - StaminaBottom * StaminaScale;
    if (HealthOpacity > .01f && Health >= 0.f)
    {
        const FVector2D Origin(27, Bottom);
        ApplyScale(LifeScale, Origin, FVector2D(27, LifeTop));
        Panel(TEXT("Health"), Origin, FVector2D(450, 113), HealthOpacity);
        const int32 Pct = FMath::RoundToInt(Health * 100.f);
        const TCHAR* Fill = Pct <= 30 ? TEXT("Red") : Pct <= 60 ? TEXT("Amber") : TEXT("Green");
        const FVector2D Track = Origin + FVector2D(112, 38);
        if (ShownHealth > .001f)
        {
            Elements.PushClip(FSlateClippingZone(Geom(Track, FVector2D(290 * ShownHealth, 18))));
            Art(Fill, Track, FVector2D(290, 18), HealthOpacity);
            Elements.PopClip();
        }
        Text(TEXT("Vida"), Origin + FVector2D(112, 62), 19.f, Opacity(Ivory, HealthOpacity));
        Text(FString::Printf(TEXT("%d%%"), Pct), Origin + FVector2D(402, 62), 19.f, Opacity(Ivory, HealthOpacity), 1.f);
        ResetScale();
    }
    if (StaminaOpacity > .01f && Stamina >= 0.f)
    {
        const FVector2D Origin(27, StaminaTop);
        ApplyScale(StaminaScale, Origin, Origin);
        Panel(TEXT("StaminaFrame"), Origin, FVector2D(450, 113), StaminaOpacity);
        const int32 Pct = Stamina > 0.f ? FMath::Max(1, FMath::RoundToInt(Stamina * 100.f)) : 0;
        const TCHAR* Fill = Pct <= 20 ? TEXT("StaminaWhite") : Pct <= 70 ? TEXT("StaminaSkyBlue") : TEXT("StaminaBlue");
        const FVector2D Track = Origin + FVector2D(109, 41);
        const FVector2D TrackSize(288, 12.5);
        if (Stamina > 0.f)
        {
            Elements.PushClip(FSlateClippingZone(Geom(Track, FVector2D(TrackSize.X * Stamina, TrackSize.Y))));
            Art(Fill, Track, TrackSize, StaminaOpacity);
            Elements.PopClip();
        }
        const FVector2D Label(113, 54);
        constexpr float LabelSize = 16.f;
        const FLinearColor LabelColor = Opacity(Ivory, StaminaOpacity);
        Elements.PushClip(FSlateClippingZone(Geom(Origin + FVector2D(110, 54), FVector2D(209, 28))));
        Text(TEXT("Resistencia"), Origin + Label, LabelSize, LabelColor);
        Elements.PopClip();
        Elements.PushClip(FSlateClippingZone(Geom(Origin + FVector2D(326, 54), FVector2D(73, 28))));
        Text(FString::Printf(TEXT("%d%%"), Pct), Origin + FVector2D(394, Label.Y), LabelSize, LabelColor, 1.f);
        Elements.PopClip();
        ResetScale();
    }
    if (BatteryOpacity > .01f && Battery >= 0.f)
    {
        const FVector2D Origin(W - 476, Bottom);
        const FVector2D Anchor = Origin + FVector2D(450, FrameHeight);
        ApplyScale(LampScale, Anchor, Anchor);
        Panel(TEXT("Lamp"), Origin, FVector2D(450, 113), BatteryOpacity);
        const int32 Pct = FMath::RoundToInt(Battery * 100.f);
        const TCHAR* Fill = Pct <= 10 ? TEXT("BatteryRed") : Pct <= 25 ? TEXT("BatteryAmber") : TEXT("BatteryWhite");
        for (int32 Index = 0; Index < 6; ++Index)
        {
            const float Charge = FMath::Clamp(Battery * 6.f - Index, 0.f, 1.f);
            if (Charge <= .001f) continue;
            const FVector2D Cell = Origin + FVector2D(134 + Index * 24.7, 37);
            Elements.PushClip(FSlateClippingZone(Geom(Cell, FVector2D(24 * Charge, 30))));
            Art(Fill, Cell, FVector2D(24, 30), BatteryOpacity);
            Elements.PopClip();
        }
        Text(FString::Printf(TEXT("%d%%"), Pct), Origin + FVector2D(377, 38), 23.f, Opacity(Ivory, BatteryOpacity), .5f);
        Text(TEXT("Linterna"), Origin + FVector2D(209, 66), 11.f, Opacity(Ivory, BatteryOpacity), .5f);
        ResetScale();
    }
    if (bGuided)
    {
        auto KeyHint = [&](const FString& Key, const TCHAR* Label, double Y)
        {
            if (Key.IsEmpty()) return;
            const FVector2D Origin(43, Y);
            Art(TEXT("Key"), Origin, FVector2D(29));
            Text(Key, Origin + FVector2D(14.5, 3), 14.f, FLinearColor(.013f, .012f, .009f), .5f);
            Text(Label, Origin + FVector2D(39, 3), 16.f, Ivory);
        };
        const double HintBottom = Stamina >= 0.f ? StaminaTop : LifeTop;
        KeyHint(MapKeyText, TEXT("Mapa completo"), HintBottom - 100);
        KeyHint(JournalKeyText, TEXT("Libreta de tareas"), HintBottom - 68);
        KeyHint(ModeKeyText, TEXT("Modo del HUD"), HintBottom - 36);
    }
    return Layer;
}
