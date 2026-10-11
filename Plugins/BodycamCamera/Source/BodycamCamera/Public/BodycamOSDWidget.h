#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Misc/DateTime.h"
#include "BodycamOSDWidget.generated.h"

class UTextBlock;
class UWidget;

/**
 * On-screen display of a body camera: timestamp, blinking REC indicator, battery and device label.
 *
 * Make a Widget Blueprint with this as parent class, lay it out in the designer and name the widgets
 * (all optional): TimestampText, RecText, RecDot, BatteryText, DeviceText.
 * Then assign the Blueprint to BodycamCameraComponent > OSD Widget Class.
 */
UCLASS(Blueprintable, BlueprintType)
class BODYCAMCAMERA_API UBodycamOSDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Text shown as "2031-10-11 T23:14:07Z". */
	UPROPERTY(BlueprintReadOnly, Category = "Bodycam|OSD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TimestampText;

	/** Text shown as "REC" (blinks together with RecDot). */
	UPROPERTY(BlueprintReadOnly, Category = "Bodycam|OSD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RecText;

	/** Any widget (e.g. a red circle image) that blinks while recording. */
	UPROPERTY(BlueprintReadOnly, Category = "Bodycam|OSD", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> RecDot;

	UPROPERTY(BlueprintReadOnly, Category = "Bodycam|OSD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> BatteryText;

	UPROPERTY(BlueprintReadOnly, Category = "Bodycam|OSD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DeviceText;

	/** Shown in DeviceText. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|OSD")
	FString DeviceLabel = TEXT("BODYCAM X81234567");

	/** Use the player's real clock instead of the in-game clock. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|OSD")
	bool bUseRealWorldTime = false;

	/** In-game date/time at world start. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|OSD", meta = (EditCondition = "!bUseRealWorldTime"))
	FDateTime StartDateTime = FDateTime(2031, 10, 11, 23, 14, 0);

	/** In-game seconds per real second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|OSD", meta = (EditCondition = "!bUseRealWorldTime"))
	float TimeScale = 1.f;

	/** Seconds per REC blink cycle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|OSD", meta = (ClampMin = "0.1"))
	float RecBlinkPeriod = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|OSD")
	bool bRecording = true;

	UFUNCTION(BlueprintCallable, Category = "Bodycam|OSD")
	void SetRecording(bool bInRecording);

	/** 0-100. */
	UFUNCTION(BlueprintCallable, Category = "Bodycam|OSD")
	void SetBatteryPercent(float Percent);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	float BatteryPercent = 100.f;
	float ElapsedSeconds = 0.f;

	void RefreshStaticTexts();
};
