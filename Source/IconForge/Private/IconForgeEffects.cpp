#include "IconForgeEffects.h"
#include "IconForgeSettings.h"
#include "Algo/Sort.h"

namespace
{
	// ------------------------------------------------------------ image helpers
	// Working format: straight alpha, sRGB-encoded floats in [0..1] (the same space as the PNG),
	// so palettes / posterize levels match what artists expect from pixel-art tools.
	struct FImg
	{
		int32 W = 0, H = 0;
		TArray<FLinearColor> P;
		void Init(int32 InW, int32 InH) { W = InW; H = InH; P.Init(FLinearColor::Transparent, W * H); }
		FLinearColor& At(int32 X, int32 Y) { return P[Y * W + X]; }
		const FLinearColor& At(int32 X, int32 Y) const { return P[Y * W + X]; }
	};

	/** User colours come from the colour picker in linear space: convert to the sRGB working space. */
	FLinearColor ToWork(const FLinearColor& Linear)
	{
		const FColor C = Linear.ToFColor(true);
		return FLinearColor(C.R / 255.f, C.G / 255.f, C.B / 255.f, FMath::Clamp(Linear.A, 0.f, 1.f));
	}

	float Luma(const FLinearColor& C) { return 0.299f * C.R + 0.587f * C.G + 0.114f * C.B; }
	float Luma(const FVector3f& C) { return 0.299f * C.X + 0.587f * C.Y + 0.114f * C.Z; }

	uint32 Hash(uint32 X, uint32 Y)
	{
		uint32 H = X * 374761393u + Y * 668265263u;
		H = (H ^ (H >> 13)) * 1274126177u;
		return H ^ (H >> 16);
	}

	/** Put Color (alpha = Mask * Color.A) UNDER the image. */
	void Under(FImg& Img, const TArray<float>& Mask, const FLinearColor& Color)
	{
		for (int32 i = 0; i < Img.P.Num(); ++i)
		{
			const float Ab = FMath::Clamp(Mask[i], 0.f, 1.f) * Color.A;
			if (Ab <= 0.f) { continue; }
			FLinearColor& T = Img.P[i];
			const float At = T.A;
			const float Ao = At + Ab * (1.f - At);
			if (Ao <= 0.f) { continue; }
			const float Wb = Ab * (1.f - At);
			T.R = (T.R * At + Color.R * Wb) / Ao;
			T.G = (T.G * At + Color.G * Wb) / Ao;
			T.B = (T.B * At + Color.B * Wb) / Ao;
			T.A = Ao;
		}
	}

	/** Put Color (alpha = Mask * Color.A) OVER the image. */
	void Over(FImg& Img, const TArray<float>& Mask, const FLinearColor& Color)
	{
		for (int32 i = 0; i < Img.P.Num(); ++i)
		{
			const float At = FMath::Clamp(Mask[i], 0.f, 1.f) * Color.A;
			if (At <= 0.f) { continue; }
			FLinearColor& B = Img.P[i];
			const float Ab = B.A;
			const float Ao = At + Ab * (1.f - At);
			const float Wb = Ab * (1.f - At);
			B.R = (Color.R * At + B.R * Wb) / Ao;
			B.G = (Color.G * At + B.G * Wb) / Ao;
			B.B = (Color.B * At + B.B * Wb) / Ao;
			B.A = Ao;
		}
	}

	TArray<float> AlphaOf(const FImg& Img)
	{
		TArray<float> A; A.SetNumUninitialized(Img.P.Num());
		for (int32 i = 0; i < Img.P.Num(); ++i) { A[i] = Img.P[i].A; }
		return A;
	}

	/** Grey-scale dilation by R pixels. Alternating square / cross kernels = octagonal (round-ish) shape. */
	TArray<float> Dilate(const TArray<float>& In, int32 W, int32 H, int32 R)
	{
		TArray<float> A = In, Tmp;
		for (int32 It = 0; It < R; ++It)
		{
			Tmp = A;
			const bool bSquare = (It % 2) == 1;
			for (int32 Y = 0; Y < H; ++Y)
			{
				for (int32 X = 0; X < W; ++X)
				{
					float M = Tmp[Y * W + X];
					for (int32 Dy = -1; Dy <= 1; ++Dy)
					{
						const int32 Yy = Y + Dy;
						if (Yy < 0 || Yy >= H) { continue; }
						for (int32 Dx = -1; Dx <= 1; ++Dx)
						{
							if (!bSquare && Dx != 0 && Dy != 0) { continue; }
							const int32 Xx = X + Dx;
							if (Xx < 0 || Xx >= W) { continue; }
							M = FMath::Max(M, Tmp[Yy * W + Xx]);
						}
					}
					A[Y * W + X] = M;
				}
			}
		}
		return A;
	}

	/** Separable box blur, 3 passes (~gaussian), O(N) per pass regardless of radius. */
	void Blur(TArray<float>& A, int32 W, int32 H, int32 Radius)
	{
		if (Radius <= 0) { return; }
		const int32 R = FMath::Max(1, Radius / 2);
		TArray<float> Tmp; Tmp.SetNumUninitialized(A.Num());
		auto Pass = [&](bool bHorizontal)
		{
			const int32 Lines = bHorizontal ? H : W, Len = bHorizontal ? W : H;
			for (int32 L = 0; L < Lines; ++L)
			{
				auto Idx = [&](int32 i) { i = FMath::Clamp(i, 0, Len - 1); return bHorizontal ? L * W + i : i * W + L; };
				float Sum = 0.f;
				for (int32 i = -R; i <= R; ++i) { const int32 J = i < 0 || i >= Len ? -1 : Idx(i); Sum += J >= 0 ? A[J] : 0.f; }
				for (int32 i = 0; i < Len; ++i)
				{
					Tmp[Idx(i)] = Sum / (2 * R + 1);
					const int32 Out = i - R, In = i + R + 1;
					if (Out >= 0) { Sum -= A[Idx(Out)]; }
					if (In < Len) { Sum += A[Idx(In)]; }
				}
			}
			Swap(A, Tmp);
		};
		for (int32 k = 0; k < 3; ++k) { Pass(true); Pass(false); }
	}

	/** Bilinear sample of premultiplied colour; outside = transparent. */
	FLinearColor SamplePremul(const FImg& Img, float X, float Y)
	{
		X -= 0.5f; Y -= 0.5f;
		const int32 X0 = FMath::FloorToInt(X), Y0 = FMath::FloorToInt(Y);
		const float Fx = X - X0, Fy = Y - Y0;
		FLinearColor Acc(0, 0, 0, 0);
		for (int32 j = 0; j < 2; ++j)
		{
			for (int32 i = 0; i < 2; ++i)
			{
				const int32 Xx = X0 + i, Yy = Y0 + j;
				if (Xx < 0 || Yy < 0 || Xx >= Img.W || Yy >= Img.H) { continue; }
				const float Wt = (i ? Fx : 1.f - Fx) * (j ? Fy : 1.f - Fy);
				const FLinearColor& C = Img.At(Xx, Yy);
				Acc.R += C.R * C.A * Wt; Acc.G += C.G * C.A * Wt; Acc.B += C.B * C.A * Wt; Acc.A += C.A * Wt;
			}
		}
		return Acc;
	}

	/** Signed distance to a rounded rectangle filling the image (negative = inside). */
	float RoundRectSDF(float X, float Y, float W, float H, float R)
	{
		R = FMath::Min(R, 0.5f * FMath::Min(W, H));
		const float Qx = FMath::Abs(X - 0.5f * W) - (0.5f * W - R);
		const float Qy = FMath::Abs(Y - 0.5f * H) - (0.5f * H - R);
		const float Ox = FMath::Max(Qx, 0.f), Oy = FMath::Max(Qy, 0.f);
		return FMath::Sqrt(Ox * Ox + Oy * Oy) + FMath::Min(FMath::Max(Qx, Qy), 0.f) - R;
	}

	// ------------------------------------------------------------ palettes (sRGB hex)

	TArray<FVector3f> FromHex(std::initializer_list<uint32> Hex)
	{
		TArray<FVector3f> Out;
		for (uint32 H : Hex) { Out.Add(FVector3f(((H >> 16) & 255) / 255.f, ((H >> 8) & 255) / 255.f, (H & 255) / 255.f)); }
		return Out;
	}

	TArray<FVector3f> BuiltinPalette(EIconForgePalette P)
	{
		switch (P)
		{
		case EIconForgePalette::GameBoy:       return FromHex({ 0x0F380F, 0x306230, 0x8BAC0F, 0x9BBC0F });
		case EIconForgePalette::GameBoyPocket: return FromHex({ 0x1C1C1C, 0x4A4A3C, 0x8C8C6E, 0xC4C4A6 });
		case EIconForgePalette::VirtualBoy:    return FromHex({ 0x000000, 0x550000, 0xAA0000, 0xFF0000 });
		case EIconForgePalette::Grayscale4:    return FromHex({ 0x000000, 0x555555, 0xAAAAAA, 0xFFFFFF });
		case EIconForgePalette::OneBit:        return FromHex({ 0x000000, 0xFFFFFF });
		case EIconForgePalette::CGA:           return FromHex({ 0x000000, 0x55FFFF, 0xFF55FF, 0xFFFFFF });
		case EIconForgePalette::PICO8:         return FromHex({ 0x000000, 0x1D2B53, 0x7E2553, 0x008751, 0xAB5236, 0x5F574F, 0xC2C3C7, 0xFFF1E8,
		                                                        0xFF004D, 0xFFA300, 0xFFEC27, 0x00E436, 0x29ADFF, 0x83769C, 0xFF77A8, 0xFFCCAA });
		case EIconForgePalette::C64:           return FromHex({ 0x000000, 0xFFFFFF, 0x68372B, 0x70A4B2, 0x6F3D86, 0x588D43, 0x352879, 0xB8C76F,
		                                                        0x6F4F25, 0x433900, 0x9A6759, 0x444444, 0x6C6C6C, 0x9AD284, 0x6C5EB5, 0x959595 });
		case EIconForgePalette::EGA:           return FromHex({ 0x000000, 0x0000AA, 0x00AA00, 0x00AAAA, 0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
		                                                        0x555555, 0x5555FF, 0x55FF55, 0x55FFFF, 0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF });
		case EIconForgePalette::Sweetie16:     return FromHex({ 0x1A1C2C, 0x5D275D, 0xB13E53, 0xEF7D57, 0xFFCD75, 0xA7F070, 0x38B764, 0x257179,
		                                                        0x29366F, 0x3B5DC9, 0x41A6F6, 0x73EFF7, 0xF4F4F4, 0x94B0C2, 0x566C86, 0x333C57 });
		case EIconForgePalette::NES:           return FromHex({
			0x7C7C7C, 0x0000FC, 0x0000BC, 0x4428BC, 0x940084, 0xA80020, 0xA81000, 0x881400, 0x503000, 0x007800, 0x006800, 0x005800, 0x004058, 0x000000,
			0xBCBCBC, 0x0078F8, 0x0058F8, 0x6844FC, 0xD800CC, 0xE40058, 0xF83800, 0xE45C10, 0xAC7C00, 0x00B800, 0x00A800, 0x00A844, 0x008888,
			0xF8F8F8, 0x3CBCFC, 0x6888FC, 0x9878F8, 0xF878F8, 0xF85898, 0xF87858, 0xFCA044, 0xF8B800, 0xB8F818, 0x58D854, 0x58F898, 0x00E8D8, 0x787878,
			0xFCFCFC, 0xA4E4FC, 0xB8B8F8, 0xD8B8F8, 0xF8B8F8, 0xF8A4C0, 0xF0D0B0, 0xFCE0A8, 0xF8D878, 0xD8F878, 0xB8F8B8, 0xB8F8D8, 0x00FCFC, 0xF8D8F8 });
		default: return {};
		}
	}

	/** Median cut on the opaque pixels. */
	TArray<FVector3f> MedianCut(const FImg& Img, int32 K)
	{
		TArray<FVector3f> Px;
		const int32 Step = FMath::Max(1, Img.P.Num() / 65536);
		for (int32 i = 0; i < Img.P.Num(); i += Step)
		{
			const FLinearColor& C = Img.P[i];
			if (C.A > 0.5f) { Px.Add(FVector3f(C.R, C.G, C.B)); }
		}
		if (Px.Num() == 0) { return FromHex({ 0x000000, 0xFFFFFF }); }

		struct FBox { int32 Begin, End; };
		TArray<FBox> Boxes; Boxes.Add({ 0, Px.Num() });
		auto Range = [&](const FBox& B, int32& OutAxis)
		{
			FVector3f Lo(1e9f), Hi(-1e9f);
			for (int32 i = B.Begin; i < B.End; ++i) { Lo = Lo.ComponentMin(Px[i]); Hi = Hi.ComponentMax(Px[i]); }
			const FVector3f D = Hi - Lo;
			OutAxis = D.X >= D.Y && D.X >= D.Z ? 0 : (D.Y >= D.Z ? 1 : 2);
			return D[OutAxis] * FMath::Sqrt((float)(B.End - B.Begin));
		};
		while (Boxes.Num() < K)
		{
			int32 Best = -1, BestAxis = 0; float BestScore = 0.f;
			for (int32 b = 0; b < Boxes.Num(); ++b)
			{
				if (Boxes[b].End - Boxes[b].Begin < 2) { continue; }
				int32 Axis; const float Score = Range(Boxes[b], Axis);
				if (Score > BestScore) { BestScore = Score; Best = b; BestAxis = Axis; }
			}
			if (Best < 0 || BestScore <= 0.f) { break; }
			FBox B = Boxes[Best];
			Algo::Sort(TArrayView<FVector3f>(Px.GetData() + B.Begin, B.End - B.Begin), [BestAxis](const FVector3f& L, const FVector3f& R) { return L[BestAxis] < R[BestAxis]; });
			const int32 Mid = (B.Begin + B.End) / 2;
			Boxes[Best] = { B.Begin, Mid };
			Boxes.Add({ Mid, B.End });
		}
		TArray<FVector3f> Out;
		for (const FBox& B : Boxes)
		{
			FVector3f Sum(0.f);
			for (int32 i = B.Begin; i < B.End; ++i) { Sum += Px[i]; }
			Out.Add(Sum / (float)FMath::Max(1, B.End - B.Begin));
		}
		return Out;
	}

	/** Colour quantizer: posterize, nearest palette colour (cached LUT) or luminance ramp. */
	struct FQuantizer
	{
		bool bPosterize = false;
		int32 Levels = 8;
		bool bByLuma = false;
		TArray<FVector3f> Palette;      // sorted by luma when bByLuma
		TArray<float> PaletteLuma;
		TArray<int16> Lut;              // 64^3 nearest-colour cache
		float Spread = 0.1f;            // typical step between colours (dither amplitude)

		void Build()
		{
			if (bPosterize) { Spread = 1.f / FMath::Max(1, Levels - 1); return; }
			if (Palette.Num() == 0) { return; }
			if (bByLuma)
			{
				Palette.Sort([](const FVector3f& A, const FVector3f& B) { return Luma(A) < Luma(B); });
				const float L0 = Luma(Palette[0]), L1 = Luma(Palette.Last());
				for (const FVector3f& C : Palette) { PaletteLuma.Add(L1 > L0 ? (Luma(C) - L0) / (L1 - L0) : 0.f); }
				Spread = 1.f / FMath::Max(1, Palette.Num() - 1);
				return;
			}
			Lut.Init(-1, 64 * 64 * 64);
			float Sum = 0.f;
			for (int32 i = 0; i < Palette.Num(); ++i)
			{
				float Best = 1e9f;
				for (int32 j = 0; j < Palette.Num(); ++j) { if (i != j) { Best = FMath::Min(Best, (Palette[i] - Palette[j]).Size()); } }
				Sum += Palette.Num() > 1 ? Best : 0.5f;
			}
			Spread = FMath::Clamp(Sum / Palette.Num(), 0.02f, 0.6f) * 0.75f;
		}

		static float Dist(const FVector3f& A, const FVector3f& B)
		{
			// "Redmean" weighted distance: cheap and much closer to perception than plain RGB
			const float Rm = 0.5f * (A.X + B.X);
			const FVector3f D = A - B;
			return (2.f + Rm) * D.X * D.X + 4.f * D.Y * D.Y + (3.f - Rm) * D.Z * D.Z;
		}

		FVector3f Quantize(const FVector3f& InC)
		{
			const FVector3f C(FMath::Clamp(InC.X, 0.f, 1.f), FMath::Clamp(InC.Y, 0.f, 1.f), FMath::Clamp(InC.Z, 0.f, 1.f));
			if (bPosterize)
			{
				const float L = (float)(Levels - 1);
				return FVector3f(FMath::RoundToFloat(C.X * L) / L, FMath::RoundToFloat(C.Y * L) / L, FMath::RoundToFloat(C.Z * L) / L);
			}
			if (Palette.Num() == 0) { return C; }
			if (bByLuma)
			{
				const float Y = Luma(C);
				int32 Best = 0; float BestD = 1e9f;
				for (int32 i = 0; i < PaletteLuma.Num(); ++i) { const float D = FMath::Abs(PaletteLuma[i] - Y); if (D < BestD) { BestD = D; Best = i; } }
				return Palette[Best];
			}
			const int32 Key = (FMath::Min(63, (int32)(C.X * 63.999f)) << 12) | (FMath::Min(63, (int32)(C.Y * 63.999f)) << 6) | FMath::Min(63, (int32)(C.Z * 63.999f));
			int16& Cached = Lut[Key];
			if (Cached < 0)
			{
				int32 Best = 0; float BestD = 1e9f;
				for (int32 i = 0; i < Palette.Num(); ++i) { const float D = Dist(C, Palette[i]); if (D < BestD) { BestD = D; Best = i; } }
				Cached = (int16)Best;
			}
			return Palette[Cached];
		}
	};

	float BayerValue(int32 N, int32 X, int32 Y)
	{
		static const int32 B2[4] = { 0, 2, 3, 1 };
		static const int32 B4[16] = { 0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5 };
		static const int32 B8[64] = {
			0, 32, 8, 40, 2, 34, 10, 42, 48, 16, 56, 24, 50, 18, 58, 26, 12, 44, 4, 36, 14, 46, 6, 38, 60, 28, 52, 20, 62, 30, 54, 22,
			3, 35, 11, 43, 1, 33, 9, 41, 51, 19, 59, 27, 49, 17, 57, 25, 15, 47, 7, 39, 13, 45, 5, 37, 63, 31, 55, 23, 61, 29, 53, 21 };
		if (N == 2) { return (B2[(Y & 1) * 2 + (X & 1)] + 0.5f) / 4.f; }
		if (N == 4) { return (B4[(Y & 3) * 4 + (X & 3)] + 0.5f) / 16.f; }
		return (B8[(Y & 7) * 8 + (X & 7)] + 0.5f) / 64.f;
	}

	// ------------------------------------------------------------ stages

	void ColorStage(FImg& Img, const FIconForgeColorFX& C)
	{
		const bool bGrade = !FMath::IsNearlyEqual(C.Brightness, 1.f) || !FMath::IsNearlyEqual(C.Gamma, 1.f) || !FMath::IsNearlyZero(C.HueShift)
			|| !FMath::IsNearlyEqual(C.Vibrance, 1.f) || C.TintAmount > 0.f;
		if (bGrade)
		{
			const FLinearColor Tint = ToWork(C.Tint);
			const float TintLuma = FMath::Max(0.05f, Luma(Tint));
			for (FLinearColor& P : Img.P)
			{
				if (P.A <= 0.f) { continue; }
				FLinearColor Q = P * C.Brightness; Q.A = P.A;
				if (!FMath::IsNearlyEqual(C.Gamma, 1.f))
				{
					Q.R = FMath::Pow(FMath::Max(Q.R, 0.f), C.Gamma); Q.G = FMath::Pow(FMath::Max(Q.G, 0.f), C.Gamma); Q.B = FMath::Pow(FMath::Max(Q.B, 0.f), C.Gamma);
				}
				if (!FMath::IsNearlyZero(C.HueShift))
				{
					FLinearColor Hsv = Q.LinearRGBToHSV();
					Hsv.R = FMath::Fmod(Hsv.R + C.HueShift + 360.f, 360.f);
					const float A = Q.A; Q = Hsv.HSVToLinearRGB(); Q.A = A;
				}
				const float Y = Luma(Q);
				if (!FMath::IsNearlyEqual(C.Vibrance, 1.f))
				{
					Q.R = Y + (Q.R - Y) * C.Vibrance; Q.G = Y + (Q.G - Y) * C.Vibrance; Q.B = Y + (Q.B - Y) * C.Vibrance;
				}
				if (C.TintAmount > 0.f)
				{
					const float K = Y / TintLuma;
					Q.R = FMath::Lerp(Q.R, Tint.R * K, C.TintAmount); Q.G = FMath::Lerp(Q.G, Tint.G * K, C.TintAmount); Q.B = FMath::Lerp(Q.B, Tint.B * K, C.TintAmount);
				}
				P = FLinearColor(FMath::Clamp(Q.R, 0.f, 1.f), FMath::Clamp(Q.G, 0.f, 1.f), FMath::Clamp(Q.B, 0.f, 1.f), P.A);
			}
		}

		if (C.Sharpen > 0.f)
		{
			// Unsharp mask, alpha-weighted so the transparent background does not create dark halos
			const FImg Src = Img;
			for (int32 Y = 0; Y < Img.H; ++Y)
			{
				for (int32 X = 0; X < Img.W; ++X)
				{
					FLinearColor& P = Img.At(X, Y);
					if (P.A <= 0.f) { continue; }
					float R = 0, G = 0, B = 0, Wt = 0;
					for (int32 Dy = -1; Dy <= 1; ++Dy)
					{
						for (int32 Dx = -1; Dx <= 1; ++Dx)
						{
							const int32 Xx = FMath::Clamp(X + Dx, 0, Img.W - 1), Yy = FMath::Clamp(Y + Dy, 0, Img.H - 1);
							const FLinearColor& N = Src.At(Xx, Yy);
							const float K = N.A * ((Dx == 0 || Dy == 0) ? 2.f : 1.f);
							R += N.R * K; G += N.G * K; B += N.B * K; Wt += K;
						}
					}
					if (Wt <= 0.f) { continue; }
					P.R = FMath::Clamp(P.R + C.Sharpen * (P.R - R / Wt), 0.f, 1.f);
					P.G = FMath::Clamp(P.G + C.Sharpen * (P.G - G / Wt), 0.f, 1.f);
					P.B = FMath::Clamp(P.B + C.Sharpen * (P.B - B / Wt), 0.f, 1.f);
				}
			}
		}
	}

	FImg Pixelate(const FImg& Img, int32 Px)
	{
		FImg Out;
		Out.Init(FMath::DivideAndRoundUp(Img.W, Px), FMath::DivideAndRoundUp(Img.H, Px));
		for (int32 Y = 0; Y < Out.H; ++Y)
		{
			for (int32 X = 0; X < Out.W; ++X)
			{
				float R = 0, G = 0, B = 0, A = 0; int32 N = 0;
				for (int32 y = Y * Px; y < FMath::Min(Img.H, (Y + 1) * Px); ++y)
				{
					for (int32 x = X * Px; x < FMath::Min(Img.W, (X + 1) * Px); ++x)
					{
						const FLinearColor& C = Img.At(x, y);
						R += C.R * C.A; G += C.G * C.A; B += C.B * C.A; A += C.A; ++N;
					}
				}
				Out.At(X, Y) = A > 0.f ? FLinearColor(R / A, G / A, B / A, A / FMath::Max(1, N)) : FLinearColor::Transparent;
			}
		}
		return Out;
	}

	void PaletteStage(FImg& Img, const FIconForgeRetroFX& R)
	{
		if (R.Palette == EIconForgePalette::None) { return; }

		// Optional luminance normalization: use the full palette range even for a dark / grey mesh
		if (R.bNormalizeLuminance)
		{
			TArray<float> L;
			for (const FLinearColor& C : Img.P) { if (C.A > 0.5f) { L.Add(Luma(C)); } }
			if (L.Num() > 16)
			{
				L.Sort();
				const float Lo = L[L.Num() * 2 / 100], Hi = L[L.Num() * 98 / 100];
				if (Hi - Lo > 0.02f)
				{
					for (FLinearColor& C : Img.P)
					{
						C.R = FMath::Clamp((C.R - Lo) / (Hi - Lo), 0.f, 1.f);
						C.G = FMath::Clamp((C.G - Lo) / (Hi - Lo), 0.f, 1.f);
						C.B = FMath::Clamp((C.B - Lo) / (Hi - Lo), 0.f, 1.f);
					}
				}
			}
		}

		FQuantizer Q;
		if (R.Palette == EIconForgePalette::Posterize) { Q.bPosterize = true; Q.Levels = FMath::Clamp(R.PosterizeLevels, 2, 64); }
		else if (R.Palette == EIconForgePalette::Auto) { Q.Palette = MedianCut(Img, FMath::Clamp(R.AutoColors, 2, 64)); }
		else if (R.Palette == EIconForgePalette::Custom)
		{
			for (const FLinearColor& C : R.CustomPalette) { const FLinearColor W = ToWork(C); Q.Palette.Add(FVector3f(W.R, W.G, W.B)); }
			if (Q.Palette.Num() == 0) { return; }
		}
		else { Q.Palette = BuiltinPalette(R.Palette); }
		Q.bByLuma = R.bMatchByLuminance && !Q.bPosterize;
		Q.Build();

		const float Strength = FMath::Max(0.f, R.DitherStrength);
		const bool bOrdered = R.Dither == EIconForgeDither::Bayer2 || R.Dither == EIconForgeDither::Bayer4 || R.Dither == EIconForgeDither::Bayer8;
		const bool bDiffuse = R.Dither == EIconForgeDither::FloydSteinberg || R.Dither == EIconForgeDither::Atkinson;
		const int32 N = R.Dither == EIconForgeDither::Bayer2 ? 2 : (R.Dither == EIconForgeDither::Bayer4 ? 4 : 8);

		TArray<FVector3f> Work; Work.SetNumUninitialized(Img.P.Num());
		for (int32 i = 0; i < Img.P.Num(); ++i) { Work[i] = FVector3f(Img.P[i].R, Img.P[i].G, Img.P[i].B); }

		auto Spread = [&](int32 X, int32 Y, const FVector3f& Err, float K)
		{
			if (X < 0 || Y < 0 || X >= Img.W || Y >= Img.H) { return; }
			const int32 i = Y * Img.W + X;
			if (Img.P[i].A > 0.f) { Work[i] += Err * K; }
		};

		for (int32 Y = 0; Y < Img.H; ++Y)
		{
			for (int32 X = 0; X < Img.W; ++X)
			{
				const int32 i = Y * Img.W + X;
				if (Img.P[i].A <= 0.f) { continue; }
				FVector3f C = Work[i];
				if (bOrdered) { C += FVector3f((BayerValue(N, X, Y) - 0.5f) * Q.Spread * Strength); }
				const FVector3f Out = Q.Quantize(C);
				if (bDiffuse)
				{
					const FVector3f Err = (Work[i] - Out) * Strength;
					if (R.Dither == EIconForgeDither::FloydSteinberg)
					{
						Spread(X + 1, Y, Err, 7.f / 16.f); Spread(X - 1, Y + 1, Err, 3.f / 16.f);
						Spread(X, Y + 1, Err, 5.f / 16.f); Spread(X + 1, Y + 1, Err, 1.f / 16.f);
					}
					else
					{
						const float K = 1.f / 8.f;
						Spread(X + 1, Y, Err, K); Spread(X + 2, Y, Err, K);
						Spread(X - 1, Y + 1, Err, K); Spread(X, Y + 1, Err, K); Spread(X + 1, Y + 1, Err, K);
						Spread(X, Y + 2, Err, K);
					}
				}
				Img.P[i].R = Out.X; Img.P[i].G = Out.Y; Img.P[i].B = Out.Z;
			}
		}
	}

	void OutlineStage(FImg& Img, const FIconForgeStylizeFX& S)
	{
		if (S.InnerEdgeDarken > 0.f)
		{
			const TArray<float> A = AlphaOf(Img);
			for (int32 Y = 0; Y < Img.H; ++Y)
			{
				for (int32 X = 0; X < Img.W; ++X)
				{
					if (A[Y * Img.W + X] < 0.5f) { continue; }
					bool bEdge = false;
					const int32 Off[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
					for (const auto& O : Off)
					{
						const int32 Xx = X + O[0], Yy = Y + O[1];
						if (Xx < 0 || Yy < 0 || Xx >= Img.W || Yy >= Img.H || A[Yy * Img.W + Xx] < 0.5f) { bEdge = true; break; }
					}
					if (bEdge) { FLinearColor& P = Img.At(X, Y); const float K = 1.f - 0.75f * S.InnerEdgeDarken; P.R *= K; P.G *= K; P.B *= K; }
				}
			}
		}
		if (S.OutlineThickness > 0)
		{
			TArray<float> D = Dilate(AlphaOf(Img), Img.W, Img.H, S.OutlineThickness);
			Under(Img, D, ToWork(S.OutlineColor));
		}
		if (S.OuterOutlineThickness > 0)
		{
			TArray<float> D = Dilate(AlphaOf(Img), Img.W, Img.H, S.OuterOutlineThickness);
			Under(Img, D, ToWork(S.OuterOutlineColor));
		}
	}

	FImg UpscaleNearest(const FImg& Small, int32 Px, int32 W, int32 H)
	{
		FImg Out; Out.Init(W, H);
		for (int32 Y = 0; Y < H; ++Y)
		{
			for (int32 X = 0; X < W; ++X) { Out.At(X, Y) = Small.At(FMath::Min(X / Px, Small.W - 1), FMath::Min(Y / Px, Small.H - 1)); }
		}
		return Out;
	}

	void GlowShadowStage(FImg& Img, const FIconForgeStylizeFX& S, float Scale)
	{
		const TArray<float> A = AlphaOf(Img);
		if (S.GlowIntensity > 0.f)
		{
			TArray<float> G = A;
			Blur(G, Img.W, Img.H, FMath::Max(1, FMath::RoundToInt(S.GlowRadius * Scale)));
			for (float& V : G) { V = FMath::Clamp(V * S.GlowIntensity * 1.5f, 0.f, 1.f); }
			FLinearColor C = ToWork(S.GlowColor); C.A = 1.f;
			Under(Img, G, C);
		}
		if (S.bDropShadow)
		{
			const int32 Ox = FMath::RoundToInt(S.ShadowOffset.X * Scale), Oy = FMath::RoundToInt(S.ShadowOffset.Y * Scale);
			TArray<float> Sh; Sh.Init(0.f, A.Num());
			for (int32 Y = 0; Y < Img.H; ++Y)
			{
				for (int32 X = 0; X < Img.W; ++X)
				{
					const int32 Sx = X - Ox, Sy = Y - Oy;
					if (Sx >= 0 && Sy >= 0 && Sx < Img.W && Sy < Img.H) { Sh[Y * Img.W + X] = A[Sy * Img.W + Sx]; }
				}
			}
			Blur(Sh, Img.W, Img.H, FMath::RoundToInt(S.ShadowBlur * Scale));
			Under(Img, Sh, ToWork(S.ShadowColor));
		}
	}

	void BackgroundStage(FImg& Img, const UIconForgeSettings& Set)
	{
		const FIconForgeStylizeFX& S = Set.StylizeFX;
		const float W = (float)Img.W, H = (float)Img.H;
		const bool bGradient = S.Background != EIconForgeBackground::FromOutput;
		const bool bSolid = !bGradient && !Set.bTransparentBackground;
		if (bGradient || bSolid)
		{
			const FLinearColor A = ToWork(bSolid ? Set.BackgroundColor : S.BackgroundA);
			const FLinearColor B = ToWork(S.BackgroundB);
			const float Ang = FMath::DegreesToRadians(S.GradientAngle);
			const float Dx = FMath::Sin(Ang), Dy = FMath::Cos(Ang);
			const float Ext = FMath::Max(1.f, FMath::Abs(W * Dx) * 0.5f + FMath::Abs(H * Dy) * 0.5f);
			const float Rad = 0.5f * FMath::Sqrt(W * W + H * H);
			for (int32 Y = 0; Y < Img.H; ++Y)
			{
				for (int32 X = 0; X < Img.W; ++X)
				{
					const float Px = X + 0.5f - 0.5f * W, Py = Y + 0.5f - 0.5f * H;
					float T = 0.f;
					if (S.Background == EIconForgeBackground::LinearGradient) { T = FMath::Clamp(0.5f + 0.5f * (Px * Dx + Py * Dy) / Ext, 0.f, 1.f); }
					else if (S.Background == EIconForgeBackground::RadialGradient) { T = FMath::Clamp(FMath::Sqrt(Px * Px + Py * Py) / Rad, 0.f, 1.f); T = T * T * (3.f - 2.f * T); }
					const FLinearColor Bg = bSolid ? A : FMath::Lerp(A, B, T);
					FLinearColor& P = Img.At(X, Y);
					const float Ao = P.A + Bg.A * (1.f - P.A);
					if (Ao <= 0.f) { continue; }
					const float Wb = Bg.A * (1.f - P.A);
					P = FLinearColor((P.R * P.A + Bg.R * Wb) / Ao, (P.G * P.A + Bg.G * Wb) / Ao, (P.B * P.A + Bg.B * Wb) / Ao, Ao);
				}
			}
		}

		const float Radius = (float)FMath::Max(0, S.CornerRadius);
		const float Thick = (float)FMath::Max(0, S.FrameThickness);
		if (Radius <= 0.f && Thick <= 0.f) { return; }
		TArray<float> FrameMask; FrameMask.Init(0.f, Img.P.Num());
		for (int32 Y = 0; Y < Img.H; ++Y)
		{
			for (int32 X = 0; X < Img.W; ++X)
			{
				const float D = RoundRectSDF(X + 0.5f, Y + 0.5f, W, H, Radius);
				const float Inside = FMath::Clamp(0.5f - D, 0.f, 1.f);
				if (Thick > 0.f) { FrameMask[Y * Img.W + X] = Inside * FMath::Clamp(0.5f + D + Thick, 0.f, 1.f); }
				if (Radius > 0.f) { Img.At(X, Y).A *= Inside; }
			}
		}
		if (Thick > 0.f) { Over(Img, FrameMask, ToWork(S.FrameColor)); }
	}

	void CRTStage(FImg& Img, const FIconForgeCRTFX& C)
	{
		const float W = (float)Img.W, H = (float)Img.H;
		if (C.Curvature > 0.f || C.ChromaticAberration > 0.f)
		{
			const FImg Src = Img;
			for (int32 Y = 0; Y < Img.H; ++Y)
			{
				for (int32 X = 0; X < Img.W; ++X)
				{
					float U = (X + 0.5f) / W * 2.f - 1.f, V = (Y + 0.5f) / H * 2.f - 1.f;
					const float R2 = U * U + V * V;
					const float K = (1.f + C.Curvature * R2) / (1.f + C.Curvature * 0.5f);   // keep the centre scale ~1
					U *= K; V *= K;
					const float Sx = (U * 0.5f + 0.5f) * W, Sy = (V * 0.5f + 0.5f) * H;
					const float Len = FMath::Max(1e-4f, FMath::Sqrt(U * U + V * V));
					const float Ca = C.ChromaticAberration * FMath::Min(1.f, Len);
					const float Ox = U / Len * Ca, Oy = V / Len * Ca;
					const FLinearColor Pr = SamplePremul(Src, Sx + Ox, Sy + Oy);
					const FLinearColor Pg = SamplePremul(Src, Sx, Sy);
					const FLinearColor Pb = SamplePremul(Src, Sx - Ox, Sy - Oy);
					const float A = FMath::Max3(Pr.A, Pg.A, Pb.A);
					Img.At(X, Y) = A > 1e-4f ? FLinearColor(Pr.R / A, Pg.G / A, Pb.B / A, FMath::Clamp(A, 0.f, 1.f)) : FLinearColor::Transparent;
				}
			}
		}

		const int32 Period = FMath::Max(2, C.ScanlinePeriod);
		for (int32 Y = 0; Y < Img.H; ++Y)
		{
			const float Phase = (float)(Y % Period) / (float)Period;   // 0..1 inside one scanline
			const float F = FMath::Sin(PI * (Phase + 0.5f / Period));           // bright in the middle of the line
			const float Scan = C.Scanlines > 0.f ? (1.f - C.Scanlines * (1.f - F * F)) * (1.f + 0.25f * C.Scanlines) : 1.f;
			for (int32 X = 0; X < Img.W; ++X)
			{
				FLinearColor& P = Img.At(X, Y);
				if (P.A <= 0.f) { continue; }
				float R = P.R * Scan, G = P.G * Scan, B = P.B * Scan;
				if (C.PhosphorMask > 0.f)
				{
					const int32 Col = X % 3;
					const float Lo = 1.f - 0.6f * C.PhosphorMask, Boost = 1.f + 0.45f * C.PhosphorMask;
					R *= (Col == 0 ? 1.f : Lo) * Boost; G *= (Col == 1 ? 1.f : Lo) * Boost; B *= (Col == 2 ? 1.f : Lo) * Boost;
				}
				if (C.Vignette > 0.f)
				{
					const float U = (X + 0.5f) / W * 2.f - 1.f, V = (Y + 0.5f) / H * 2.f - 1.f;
					const float D = FMath::Clamp((FMath::Sqrt(U * U + V * V) / 1.4142f - 0.35f) / 0.65f, 0.f, 1.f);
					const float K = 1.f - C.Vignette * D * D;
					R *= K; G *= K; B *= K;
				}
				if (C.Noise > 0.f)
				{
					const float N = ((Hash((uint32)X, (uint32)Y) & 0xFFFF) / 65535.f * 2.f - 1.f) * C.Noise;
					R += N; G += N; B += N;
				}
				P.R = FMath::Clamp(R, 0.f, 1.f); P.G = FMath::Clamp(G, 0.f, 1.f); P.B = FMath::Clamp(B, 0.f, 1.f);
			}
		}
	}
}

namespace IconForgeFX
{
	FIntPoint GetOutputSize(const UIconForgeSettings& S)
	{
		const int32 Px = FMath::Max(1, S.RetroFX.PixelSize);
		if (Px > 1 && S.RetroFX.bOutputNativePixels)
		{
			return FIntPoint(FMath::DivideAndRoundUp(S.Width, Px), FMath::DivideAndRoundUp(S.Height, Px));
		}
		return FIntPoint(S.Width, S.Height);
	}

	bool HasAnyEffect(const UIconForgeSettings& S)
	{
		const FIconForgeColorFX& C = S.ColorFX; const FIconForgeRetroFX& R = S.RetroFX;
		const FIconForgeStylizeFX& St = S.StylizeFX; const FIconForgeCRTFX& T = S.CRTFX;
		return !FMath::IsNearlyEqual(C.Brightness, 1.f) || !FMath::IsNearlyEqual(C.Gamma, 1.f) || !FMath::IsNearlyZero(C.HueShift)
			|| !FMath::IsNearlyEqual(C.Vibrance, 1.f) || C.TintAmount > 0.f || C.Sharpen > 0.f
			|| R.PixelSize > 1 || R.bHardAlpha || R.Palette != EIconForgePalette::None
			|| St.OutlineThickness > 0 || St.OuterOutlineThickness > 0 || St.InnerEdgeDarken > 0.f || St.bDropShadow || St.GlowIntensity > 0.f
			|| St.Background != EIconForgeBackground::FromOutput || St.FrameThickness > 0 || St.CornerRadius > 0
			|| T.Scanlines > 0.f || T.PhosphorMask > 0.f || T.ChromaticAberration > 0.f || T.Curvature > 0.f || T.Vignette > 0.f || T.Noise > 0.f;
	}

	void Process(const UIconForgeSettings& S, TArray<FColor>& Pixels, int32& W, int32& H)
	{
		if (W <= 0 || H <= 0 || Pixels.Num() != W * H) { return; }

		FImg Img; Img.Init(W, H);
		for (int32 i = 0; i < Pixels.Num(); ++i)
		{
			const FColor& C = Pixels[i];
			Img.P[i] = FLinearColor(C.R / 255.f, C.G / 255.f, C.B / 255.f, C.A / 255.f);
		}

		// 1. Colour grading at full resolution
		ColorStage(Img, S.ColorFX);

		// 2. Retro: work on the "art pixel" grid so outlines and dithering are pixel-perfect
		const int32 Px = FMath::Clamp(S.RetroFX.PixelSize, 1, 64);
		FImg Art = Px > 1 ? Pixelate(Img, Px) : MoveTemp(Img);
		if (S.RetroFX.bHardAlpha)
		{
			const float T = S.RetroFX.AlphaThreshold;
			for (FLinearColor& C : Art.P) { C.A = C.A >= T ? 1.f : 0.f; }
		}
		PaletteStage(Art, S.RetroFX);
		OutlineStage(Art, S.StylizeFX);

		// 3. Back to output resolution (or keep the native sprite)
		const bool bNative = Px > 1 && S.RetroFX.bOutputNativePixels;
		FImg Out = (Px > 1 && !bNative) ? UpscaleNearest(Art, Px, W, H) : MoveTemp(Art);
		const float Scale = bNative ? 1.f / Px : 1.f;   // offsets / radii are authored in output pixels

		// 4. Glow + shadow, background, frame, CRT
		GlowShadowStage(Out, S.StylizeFX, Scale);
		BackgroundStage(Out, S);
		CRTStage(Out, S.CRTFX);

		W = Out.W; H = Out.H;
		Pixels.SetNumUninitialized(W * H);
		for (int32 i = 0; i < Out.P.Num(); ++i)
		{
			const FLinearColor& C = Out.P[i];
			const uint8 A = (uint8)FMath::Clamp(FMath::RoundToInt(C.A * 255.f), 0, 255);
			Pixels[i] = A == 0 ? FColor(0, 0, 0, 0) : FColor(
				(uint8)FMath::Clamp(FMath::RoundToInt(C.R * 255.f), 0, 255),
				(uint8)FMath::Clamp(FMath::RoundToInt(C.G * 255.f), 0, 255),
				(uint8)FMath::Clamp(FMath::RoundToInt(C.B * 255.f), 0, 255), A);
		}
	}
}
