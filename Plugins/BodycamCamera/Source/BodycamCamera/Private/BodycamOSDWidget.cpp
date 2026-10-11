#include "BodycamOSDWidget.h"

#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Misc/Timespan.h"

void UBodycamOSDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ElapsedSeconds = 0.f;
	RefreshStaticTexts();
}

void UBodycamOSDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	ElapsedSeconds += InDeltaTime;

	if (TimestampText)
	{
		const FDateTime Now = bUseRealWorldTime
			? FDateTime::Now()
			: StartDateTime + FTimespan::FromSeconds(static_cast<double>(ElapsedSeconds) * TimeScale);
		TimestampText->SetText(FText::FromString(Now.ToString(TEXT("%Y-%m-%d T%H:%M:%SZ"))));
	}

	const bool bBlinkOn = bRecording && FMath::Fmod(ElapsedSeconds, RecBlinkPeriod) < RecBlinkPeriod * 0.6f;
	const ESlateVisibility BlinkVisibility = bBlinkOn ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden;
	if (RecDot)
	{
		RecDot->SetVisibility(BlinkVisibility);
	}
	if (RecText)
	{
		RecText->SetVisibility(BlinkVisibility);
	}
}

void UBodycamOSDWidget::SetRecording(bool bInRecording)
{
	bRecording = bInRecording;
}

void UBodycamOSDWidget::SetBatteryPercent(float Percent)
{
	BatteryPercent = FMath::Clamp(Percent, 0.f, 100.f);
	RefreshStaticTexts();
}

void UBodycamOSDWidget::RefreshStaticTexts()
{
	if (DeviceText)
	{
		DeviceText->SetText(FText::FromString(DeviceLabel));
	}
	if (RecText)
	{
		RecText->SetText(FText::FromString(TEXT("REC")));
	}
	if (BatteryText)
	{
		BatteryText->SetText(FText::FromString(FString::Printf(TEXT("BAT %d%%"), FMath::RoundToInt(BatteryPercent))));
	}
}
