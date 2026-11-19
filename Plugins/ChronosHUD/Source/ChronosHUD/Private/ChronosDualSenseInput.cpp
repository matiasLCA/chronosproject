#include "ChronosDualSenseInput.h"

#if PLATFORM_WINDOWS
#include "GenericPlatform/GenericApplicationMessageHandler.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "GenericPlatform/InputDeviceRegistry.h"
#include "HAL/PlatformTime.h"
#include "IInputDevice.h"
#include "Misc/CoreMisc.h"
#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include <hidsdi.h>
#include <hidpi.h>
#include <setupapi.h>
#include "Windows/HideWindowsPlatformTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogChronosDualSense, Log, All);

namespace
{
    constexpr uint16 Sony = 0x054c;
    constexpr uint16 DualSense = 0x0ce6;
    constexpr uint8 StandardReport = 0x01;
    constexpr uint8 BluetoothReport = 0x31;
    constexpr uint32 BluetoothReportLength = 78;
    constexpr uint32 USBReportLength = 64;

    enum EPadButton : uint32
    {
        Square = 1 << 0, Cross = 1 << 1, Circle = 1 << 2, Triangle = 1 << 3,
        L1 = 1 << 4, R1 = 1 << 5, L2 = 1 << 6, R2 = 1 << 7,
        Create = 1 << 8, Options = 1 << 9, L3 = 1 << 10, R3 = 1 << 11,
        Up = 1 << 12, Right = 1 << 13, Down = 1 << 14, Left = 1 << 15
    };

    const FName ButtonKeys[] = {
        FGamepadKeyNames::FaceButtonLeft, FGamepadKeyNames::FaceButtonBottom,
        FGamepadKeyNames::FaceButtonRight, FGamepadKeyNames::FaceButtonTop,
        FGamepadKeyNames::LeftShoulder, FGamepadKeyNames::RightShoulder,
        FGamepadKeyNames::LeftTriggerThreshold, FGamepadKeyNames::RightTriggerThreshold,
        FGamepadKeyNames::SpecialLeft, FGamepadKeyNames::SpecialRight,
        FGamepadKeyNames::LeftThumb, FGamepadKeyNames::RightThumb,
        FGamepadKeyNames::DPadUp, FGamepadKeyNames::DPadRight,
        FGamepadKeyNames::DPadDown, FGamepadKeyNames::DPadLeft
    };

    const FName AxisKeys[] = {
        FGamepadKeyNames::LeftAnalogX, FGamepadKeyNames::LeftAnalogY,
        FGamepadKeyNames::RightAnalogX, FGamepadKeyNames::RightAnalogY,
        FGamepadKeyNames::LeftTriggerAnalog, FGamepadKeyNames::RightTriggerAnalog
    };

    struct FPadState
    {
        float Axes[UE_ARRAY_COUNT(AxisKeys)]{};
        uint32 Buttons = 0;
    };

    float Stick(uint8 Value)
    {
        const float Normalized = (2.f * Value / 255.f) - 1.f;
        return FMath::Abs(Normalized) < .04f ? 0.f : Normalized;
    }

    bool Decode(const uint8* Report, uint32 Size, FPadState& State)
    {
        if (Size < 10) return false;
        const bool bFullBluetooth = Report[0] == BluetoothReport && Size == BluetoothReportLength;
        const bool bUSB = Report[0] == StandardReport && Size == USBReportLength;
        const bool bSimple = Report[0] == StandardReport && (Size == 10 || Size == BluetoothReportLength);
        if (!bFullBluetooth && !bUSB && !bSimple) return false;

        // Extended Bluetooth reports have one header byte more than extended USB reports.
        // The simple Bluetooth report places its trigger readings after the buttons.
        const uint8* Payload = Report + (bFullBluetooth ? 2 : 1);
        const uint8* Buttons = Payload + (bSimple ? 4 : 7);
        const uint8* Triggers = Payload + (bSimple ? 7 : 4);
        State.Axes[0] = Stick(Payload[0]);
        State.Axes[1] = -Stick(Payload[1]);
        State.Axes[2] = Stick(Payload[2]);
        State.Axes[3] = -Stick(Payload[3]);
        State.Axes[4] = Triggers[0] / 255.f;
        State.Axes[5] = Triggers[1] / 255.f;
        State.Buttons = (Buttons[0] >> 4) | (static_cast<uint32>(Buttons[1]) << 4);
        static constexpr uint32 Directions[] = {Up, Up | Right, Right, Right | Down, Down, Down | Left, Left, Left | Up};
        const uint8 Hat = Buttons[0] & 0x0f;
        if (Hat < UE_ARRAY_COUNT(Directions)) State.Buttons |= Directions[Hat];
        return true;
    }

    class FChronosDualSenseInput final : public IInputDevice
    {
    public:
        explicit FChronosDualSenseInput(const TSharedRef<FGenericApplicationMessageHandler>& InHandler) : Handler(InHandler) {}
        ~FChronosDualSenseInput() override { ReleaseHandle(); }

        void Tick(float) override {}
        void SetMessageHandler(const TSharedRef<FGenericApplicationMessageHandler>& InHandler) override { Handler = InHandler; }
        bool Exec(UWorld*, const TCHAR*, FOutputDevice&) override { return false; }
        void SetChannelValue(int32, FForceFeedbackChannelType, float) override {}
        void SetChannelValues(int32, const FForceFeedbackValues&) override {}
        bool IsGamepadAttached() const override { return User.IsValid(); }

        void SendControllerEvents() override
        {
            if (IsEngineExitRequested()) return;
            const double Now = FPlatformTime::Seconds();
            if (Input == INVALID_HANDLE_VALUE)
            {
                if (Now < RetryAt) return;
                RetryAt = Now + 1.;
                if (!Open()) return;
                LastReport = Now;
            }

            for (int32 Index = 0; Index < 32; ++Index)
            {
                DWORD Size = 0;
                if (!PendingRead)
                {
                    ResetEvent(Request.hEvent);
                    if (!ReadFile(Input, Report, ReportLength, nullptr, &Request) && GetLastError() != ERROR_IO_PENDING)
                    {
                        Close();
                        return;
                    }
                    PendingRead = true;
                }
                if (!GetOverlappedResult(Input, &Request, &Size, false))
                {
                    if (GetLastError() == ERROR_IO_INCOMPLETE) break;
                    Close();
                    return;
                }
                PendingRead = false;

                FPadState State;
                if (!Decode(Report, Size, State)) continue;
                LastReport = Now;
                if (!User.IsValid()) Connect();
                SendButtons(State.Buttons, Now);
                Latest = State;
            }

            if (Now - LastReport > 2.)
            {
                Close();
                return;
            }

            if (!User.IsValid()) return;
            for (int32 Index = 0; Index < UE_ARRAY_COUNT(AxisKeys); ++Index)
            {
                if (Latest.Axes[Index] != 0.f || LastAxes[Index] != 0.f)
                    Handler->OnControllerAnalog(AxisKeys[Index], User, Device, Latest.Axes[Index]);
                LastAxes[Index] = Latest.Axes[Index];
            }
            for (int32 Index = 12; Index < UE_ARRAY_COUNT(ButtonKeys); ++Index)
                if ((Pressed & (1u << Index)) && Now >= RepeatAt[Index])
                {
                    Handler->OnControllerButtonPressed(ButtonKeys[Index], User, Device, true);
                    RepeatAt[Index] = Now + .10;
                }
        }

    private:
        bool Open()
        {
            GUID Guid{};
            HidD_GetHidGuid(&Guid);
            const HDEVINFO Interfaces = SetupDiGetClassDevsW(&Guid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
            if (Interfaces == INVALID_HANDLE_VALUE) return false;

            for (DWORD Index = 0; Input == INVALID_HANDLE_VALUE; ++Index)
            {
                SP_DEVICE_INTERFACE_DATA Interface{};
                Interface.cbSize = sizeof(Interface);
                if (!SetupDiEnumDeviceInterfaces(Interfaces, nullptr, &Guid, Index, &Interface)) break;
                DWORD DetailSize = 0;
                SetupDiGetDeviceInterfaceDetailW(Interfaces, &Interface, nullptr, 0, &DetailSize, nullptr);
                if (DetailSize < sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W)) continue;

                TArray<uint8> Buffer;
                Buffer.SetNumUninitialized(DetailSize);
                auto* Detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(Buffer.GetData());
                Detail->cbSize = sizeof(*Detail);
                if (!SetupDiGetDeviceInterfaceDetailW(Interfaces, &Interface, Detail, DetailSize, nullptr, nullptr)) continue;
                TryOpen(Detail->DevicePath);
            }
            SetupDiDestroyDeviceInfoList(Interfaces);
            return Input != INVALID_HANDLE_VALUE;
        }

        void TryOpen(const TCHAR* Path)
        {
            constexpr DWORD Share = FILE_SHARE_READ | FILE_SHARE_WRITE;
            const HANDLE Metadata = CreateFileW(Path, 0, Share, nullptr, OPEN_EXISTING, 0, nullptr);
            if (Metadata == INVALID_HANDLE_VALUE) return;
            HIDD_ATTRIBUTES Attributes{};
            Attributes.Size = sizeof(Attributes);
            PHIDP_PREPARSED_DATA Parsed = nullptr;
            HIDP_CAPS Caps{};
            const bool bGamepad = HidD_GetAttributes(Metadata, &Attributes)
                && Attributes.VendorID == Sony && Attributes.ProductID == DualSense
                && HidD_GetPreparsedData(Metadata, &Parsed) && HidP_GetCaps(Parsed, &Caps) == HIDP_STATUS_SUCCESS
                && Caps.UsagePage == 1 && Caps.Usage == 5;
            if (Parsed) HidD_FreePreparsedData(Parsed);
            CloseHandle(Metadata);

            if (!bGamepad || (Caps.InputReportByteLength != USBReportLength &&
                Caps.InputReportByteLength != BluetoothReportLength && Caps.InputReportByteLength != 10)) return;
            Input = CreateFileW(Path, GENERIC_READ, Share, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
            if (Input == INVALID_HANDLE_VALUE) return;
            ReportLength = Caps.InputReportByteLength;
            Request = {};
            Request.hEvent = CreateEventW(nullptr, true, false, nullptr);
            if (!Request.hEvent) Close();
        }

        void Connect()
        {
            auto& Mapper = IPlatformInputDeviceMapper::Get();
            if (!Device.IsValid()) Device = Mapper.AllocateNewInputDeviceId();
            User = Mapper.GetPlatformUserForNewlyConnectedDevice();
            FInputDeviceDescriptor Descriptor;
            Descriptor.HardwareDeviceHandle = Device;
            Descriptor.InputDeviceName = TEXT("ChronosHID");
            Descriptor.HardwareDeviceIdentifier = TEXT("DualSense");
            FInputDeviceRegistry::RegisterDevice(Descriptor);
            Mapper.Internal_MapInputDeviceToUser(Device, User, EInputDeviceConnectionState::Connected);
            UE_LOG(LogChronosDualSense, Display, TEXT("DualSense HID connected (%s), device %d, player %d."),
                ReportLength == USBReportLength ? TEXT("USB") : TEXT("Bluetooth"), Device.GetId(), User.GetInternalId());
        }

        void SendButtons(uint32 Current, double Now)
        {
            const uint32 Changed = Current ^ Pressed;
            for (int32 Index = 0; Index < UE_ARRAY_COUNT(ButtonKeys); ++Index)
            {
                const uint32 Mask = 1u << Index;
                if (!(Changed & Mask)) continue;
                if (Current & Mask)
                {
                    Handler->OnControllerButtonPressed(ButtonKeys[Index], User, Device, false);
                    RepeatAt[Index] = Now + .32;
                }
                else Handler->OnControllerButtonReleased(ButtonKeys[Index], User, Device, false);
            }
            if (Changed) UE_LOG(LogChronosDualSense, Verbose, TEXT("DualSense buttons: %04x."), Current);
            Pressed = Current;
        }

        void ReleaseHandle()
        {
            if (Input != INVALID_HANDLE_VALUE)
            {
                if (PendingRead)
                {
                    CancelIoEx(Input, &Request);
                    DWORD Ignored = 0;
                    GetOverlappedResult(Input, &Request, &Ignored, true);
                }
                CloseHandle(Input);
                Input = INVALID_HANDLE_VALUE;
            }
            if (Request.hEvent) CloseHandle(Request.hEvent);
            Request = {};
            PendingRead = false;
        }

        void Close()
        {
            ReleaseHandle();
            if (!User.IsValid() || IsEngineExitRequested()) return;

            SendButtons(0, 0.);
            for (int32 Index = 0; Index < UE_ARRAY_COUNT(AxisKeys); ++Index)
                if (LastAxes[Index] != 0.f) Handler->OnControllerAnalog(AxisKeys[Index], User, Device, 0.f);
            Latest = {};
            FMemory::Memzero(LastAxes);
            IPlatformInputDeviceMapper::Get().Internal_MapInputDeviceToUser(Device, User, EInputDeviceConnectionState::Disconnected);
            User = PLATFORMUSERID_NONE;
            UE_LOG(LogChronosDualSense, Display, TEXT("DualSense HID disconnected (device %d)."), Device.GetId());
        }

        TSharedRef<FGenericApplicationMessageHandler> Handler;
        HANDLE Input = INVALID_HANDLE_VALUE;
        OVERLAPPED Request{};
        bool PendingRead = false;
        uint8 Report[BluetoothReportLength]{};
        uint32 ReportLength = 0;
        FPadState Latest;
        float LastAxes[UE_ARRAY_COUNT(AxisKeys)]{};
        double RepeatAt[UE_ARRAY_COUNT(ButtonKeys)]{};
        uint32 Pressed = 0;
        double LastReport = 0.;
        double RetryAt = 0.;
        FPlatformUserId User = PLATFORMUSERID_NONE;
        FInputDeviceId Device = INPUTDEVICEID_NONE;
    };
}

TSharedPtr<IInputDevice> CreateChronosDualSenseInput(const TSharedRef<FGenericApplicationMessageHandler>& Handler)
{
    return MakeShared<FChronosDualSenseInput>(Handler);
}
#endif
