#pragma once

#include "CoreMinimal.h"
#include "Style/IconForgeStyle.h"
#include "Styling/AppStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Text/STextBlock.h"

/** Small reusable widgets built on FIconForgeStyle (inline: safe with and without unity builds). */
namespace IconForgeUI
{
	inline const ISlateStyle& St() { return FIconForgeStyle::Get(); }

	inline TSharedRef<SWidget> Caption(const TAttribute<FText>& Text)
	{
		return SNew(STextBlock).TextStyle(&St(), "IconForge.Text.Caption").Text(Text);
	}

	inline TSharedRef<SWidget> Dot(const TAttribute<FSlateColor>& Color, float Size)
	{
		return SNew(SBox).WidthOverride(Size).HeightOverride(Size)
			[
				SNew(SImage).Image(FIconForgeStyle::Brush("IconForge.Circle")).ColorAndOpacity(Color)
			];
	}

	/** Text button, optionally with an editor icon in front. Not focusable, so Space/Enter keep meaning "Shot!". */
	inline TSharedRef<SWidget> Button(const TAttribute<FText>& Label, FOnClicked OnClicked,
		const TAttribute<FText>& Tooltip = TAttribute<FText>(), FName Style = "IconForge.Button", FName Icon = NAME_None)
	{
		TSharedRef<SHorizontalBox> Content = SNew(SHorizontalBox);
		if (!Icon.IsNone())
		{
			Content->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
			[
				SNew(SBox).WidthOverride(14.f).HeightOverride(14.f)
				[
					SNew(SImage).Image(FAppStyle::GetBrush(Icon)).ColorAndOpacity(FIconForgeStyle::TextMuted())
				]
			];
		}
		Content->AddSlot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(STextBlock).TextStyle(&St(), "IconForge.Text.Button").Text(Label)
		];

		return SNew(SButton)
			.ButtonStyle(&St(), Style)
			.IsFocusable(false)
			.ToolTipText(Tooltip)
			.HAlign(HAlign_Center).VAlign(VAlign_Center)
			.OnClicked(OnClicked)
			[
				Content
			];
	}

	/** Toggle chip. */
	inline TSharedRef<SWidget> Chip(const TAttribute<FText>& Label, TFunction<bool()> IsOn, TFunction<void(bool)> OnSet,
		const TAttribute<FText>& Tooltip = TAttribute<FText>())
	{
		return SNew(SCheckBox)
			.Style(&St(), "IconForge.Chip")
			.IsFocusable(false)
			.ToolTipText(Tooltip)
			.IsChecked_Lambda([IsOn]() { return IsOn() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			.OnCheckStateChanged_Lambda([OnSet](ECheckBoxState State) { OnSet(State == ECheckBoxState::Checked); })
			[
				SNew(STextBlock).TextStyle(&St(), "IconForge.Text.Button").Text(Label)
			];
	}

	/** Coloured pill with bold text (status / badges). */
	inline TSharedRef<SWidget> Pill(const TAttribute<FText>& Text, const FLinearColor& Color)
	{
		FLinearColor Bg = Color; Bg.A = 0.16f;
		return SNew(SBorder)
			.BorderImage(FIconForgeStyle::Brush("IconForge.Pill"))
			.BorderBackgroundColor(Bg)
			.Padding(FMargin(9.f, 3.f))
			[
				SNew(STextBlock).Font(FIconForgeStyle::Font("Bold", 8)).ColorAndOpacity(Color).Text(Text)
			];
	}

	/** Rounded panel with a CAPTION title row and optional widget on the right of the title. */
	inline TSharedRef<SWidget> Panel(const TAttribute<FText>& Title, TSharedRef<SWidget> Content, TSharedPtr<SWidget> TitleExtra = nullptr)
	{
		return SNew(SBorder)
			.BorderImage(FIconForgeStyle::Brush("IconForge.Panel"))
			.Padding(1.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(14.f, 10.f, 14.f, 8.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						Caption(Title)
					]
					+ SHorizontalBox::Slot().FillWidth(1.f)[ SNew(SSpacer) ]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						TitleExtra.IsValid() ? TitleExtra.ToSharedRef() : SNullWidget::NullWidget
					]
				]
				+ SVerticalBox::Slot().FillHeight(1.f)
				[
					Content
				]
			];
	}

	/** "LABEL   [content]" row used in the quick settings block. */
	inline TSharedRef<SWidget> Row(const FText& Label, TSharedRef<SWidget> Content)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0.f, 5.f, 10.f, 0.f)
			[
				SNew(SBox).WidthOverride(74.f)
				[
					Caption(Label)
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				Content
			];
	}
}
