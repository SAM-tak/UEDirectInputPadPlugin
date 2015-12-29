#include "DirectInputPadPluginPrivatePCH.h"

#if WITH_EDITOR
#include "MainFrame.h"
#endif

#include "DirectInputPadJoystick.h"

#include "DirectInputDriver.h"
#include "DirectInputJoystick.h"

#include "DirectInputPadDevice.h"

DEFINE_LOG_CATEGORY_STATIC(DirectInputPadPlugin, Log, All)

namespace{
const uint32 MAX_JOYSTICKS = 8;
}

#include "AllowWindowsPlatformTypes.h"

bool FDirectInputPadDevice::Init()
{
	UDirectInputPadFunctionLibrary::InitDirectInputPadJoystickLibrary();

	// MainWindowのウインドウハンドルを取得する
	HWND hWnd = NULL;
	TSharedPtr<SWindow> MainWindow;

#if WITH_EDITOR
	if(GIsEditor)
	{
		auto& MainFrameModule = IMainFrameModule::Get();
		MainWindow = MainFrameModule.GetParentWindow();
	}
	else
#endif
	{
		MainWindow = GEngine->GameViewport->GetWindow();
	}

	if(MainWindow.IsValid() && MainWindow->GetNativeWindow().IsValid())
	{
		hWnd = static_cast<HWND>(MainWindow->GetNativeWindow()->GetOSWindowHandle());
	}

	DDriver_ = MakeShareable<FDirectInputDriver>(new FDirectInputDriver());
	if(!DDriver_->Init()) return false;

	DFactory_ = MakeShareable<FDirectInputJoystickFactory>(new FDirectInputJoystickFactory());
	if(!DFactory_->Init(hWnd, DDriver_)) return false;

	XInputDeviceNum_ = DFactory_->GetXInputDeviceNum();
	DInputDeviceNum_ = 0;
	DJoysticks_.SetNum(MAX_JOYSTICKS);
	
	uint32 DJoyNum = MAX_JOYSTICKS - XInputDeviceNum_;
	DJoyNum =( DFactory_->EnabledJoystickNum() < DJoyNum) ? DFactory_->EnabledJoystickNum() : DJoyNum;

	UE_LOG(DirectInputPadPlugin, Log, TEXT("DirectInputPad detected: %d"), DJoyNum);

	for(uint32 i=0; i<DJoyNum; ++i)
	{
		//UE_LOG(DirectInputPadPlugin, Log, TEXT("DIPad %d"),i);

		auto joy = DFactory_->GetJoystick(i);
		if(joy.IsValid())
		{
			joy->SetPlayerIndex(XInputDeviceNum_+i);
			DJoysticks_[XInputDeviceNum_+i] = joy;

			++DInputDeviceNum_;
		}
	}

	return true;
}

void FDirectInputPadDevice::Fin()
{
	UDirectInputPadFunctionLibrary::FinDirectInputPadJoystickLibrary();
	DFactory_->Fin();
	DDriver_->Fin();
}

void FDirectInputPadDevice::SendControllerEvents()
{
	if(DInputDeviceNum_==0) return;

	for(const auto& j : DJoysticks_)
	{
		if(j.IsValid())
		{
			// まずは入力チェック
			const auto& Joystick = j.Pin();
			if(Joystick->Input())
			{// 入力に合わせてイベントを飛ばす
				Joystick->Event(MessageHandler_);
			}
		}
	}
}

TWeakPtr<FDirectInputJoystick> FDirectInputPadDevice::GetJoystick(uint32 nPlayerID)
{
	if(!DJoysticks_.IsValidIndex(nPlayerID))
		return nullptr;

	if(DJoysticks_[nPlayerID].IsValid()) 
		return DJoysticks_[nPlayerID];

	return nullptr;
}

#include "HideWindowsPlatformTypes.h"
