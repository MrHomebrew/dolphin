// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// This file is UGLY (full of #ifdef) so that it can be used with both GC and
// Wii version of AX. Maybe it would be better to abstract away the parts that
// can be made common.

#pragma once

#if !defined(AX_GC) && !defined(AX_WII)
#error AXVoice.h included without specifying version
#endif

#include <algorithm>
#include <functional>
#include <memory>

#include "Common/CommonTypes.h"
#include "Core/DSP/DSPAccelerator.h"
#include "Core/HW/DSP.h"
#include "Core/HW/DSPHLE/UCodes/AX.h"
#include "Core/HW/DSPHLE/UCodes/AXStructs.h"
#include "Core/HW/Memmap.h"

namespace DSP
{
namespace HLE
{
#ifdef AX_GC
#define PB_TYPE AXPB
#define MAX_SAMPLES_PER_FRAME 32
#else
#define PB_TYPE AXPBWii
#define MAX_SAMPLES_PER_FRAME 96
#endif

// Put all of that in an anonymous namespace to avoid stupid compilers merging
// functions from AX GC and AX Wii.
namespace
{
// Useful macro to convert xxx_hi + xxx_lo to xxx for 32 bits.
#define HILO_TO_32(name) ((name##_hi << 16) | name##_lo)

// Used to pass a large amount of buffers to the mixing function.
union AXBuffers
{
  struct
  {
    int* left;
    int* right;
    int* surround;

    int* auxA_left;
    int* auxA_right;
    int* auxA_surround;

    int* auxB_left;
    int* auxB_right;
    int* auxB_surround;

#ifdef AX_WII
    int* auxC_left;
    int* auxC_right;
    int* auxC_surround;

    int* wm_main0;
    int* wm_aux0;
    int* wm_main1;
    int* wm_aux1;
    int* wm_main2;
    int* wm_aux2;
    int* wm_main3;
    int* wm_aux3;
#endif
  };

#ifdef AX_GC
  int* ptrs[9];
#else
  int* ptrs[20];
#endif
};

// Determines if this version of the UCode has a PBLowPassFilter in its AXPB layout.
bool HasLpf(u32 crc)
{
  switch (crc)
  {
  case 0x4E8A8B21:
    return false;
  default:
    return true;
  }
}

// Read a PB from MRAM/ARAM
void ReadPB(u32 addr, PB_TYPE& pb, u32 crc)
{
  if (HasLpf(crc))
  {
    u16* dst = (u16*)&pb;
    Memory::CopyFromEmuSwapped<u16>(dst, addr, sizeof(pb));
  }
  else
  {
    // The below is a terrible hack in order to support two different AXPB layouts.
    // We skip lpf in this layout.

    char* dst = (char*)&pb;

    constexpr size_t lpf_off = offsetof(AXPB, lpf);
    constexpr size_t lc_off = offsetof(AXPB, loop_counter);

    Memory::CopyFromEmuSwapped<u16>((u16*)dst, addr, lpf_off);
    memset(dst + lpf_off, 0, lc_off - lpf_off);
    Memory::CopyFromEmuSwapped<u16>((u16*)(dst + lc_off), addr + lpf_off, sizeof(pb) - lc_off);
  }
}

// Write a PB back to MRAM/ARAM
void WritePB(u32 addr, const PB_TYPE& pb, u32 crc)
{
  if (HasLpf(crc))
  {
    const u16* src = (const u16*)&pb;
    Memory::CopyToEmuSwapped<u16>(addr, src, sizeof(pb));
  }
  else
  {
    // The below is a terrible hack in order to support two different AXPB layouts.
    // We skip lpf in this layout.

    const char* src = (const char*)&pb;

    constexpr size_t lpf_off = offsetof(AXPB, lpf);
    constexpr size_t lc_off = offsetof(AXPB, loop_counter);

    Memory::CopyToEmuSwapped<u16>(addr, (const u16*)src, lpf_off);
    Memory::CopyToEmuSwapped<u16>(addr + lpf_off, (const u16*)(src + lc_off), sizeof(pb) - lc_off);
  }
}

#if 0
// Dump the value of a PB for debugging
#define DUMP_U16(field) WARN_LOG(DSPHLE, "    %04x (%s)", pb.field, #field)
#define DUMP_U32(field) WARN_LOG(DSPHLE, "    %08x (%s)", HILO_TO_32(pb.field), #field)
void DumpPB(const PB_TYPE& pb)
{
	DUMP_U32(next_pb);
	DUMP_U32(this_pb);
	DUMP_U16(src_type);
	DUMP_U16(coef_select);
#ifdef AX_GC
	DUMP_U16(mixer_control);
#else
	DUMP_U32(mixer_control);
#endif
	DUMP_U16(running);
	DUMP_U16(is_stream);

	// TODO: complete as needed
}
#endif

// Simulated accelerator state.
static PB_TYPE* acc_pb;
static bool acc_end_reached;

class HLEAccelerator final : public Accelerator
{
protected:
  void OnEndException() override
  {
    if (acc_pb->audio_addr.looping)
    {
      // Set the ADPCM info to continue processing at loop_addr.
      SetPredScale(acc_pb->adpcm_loop_info.pred_scale);
      if (!acc_pb->is_stream)
      {
        SetYn1(acc_pb->adpcm_loop_info.yn1);
        SetYn2(acc_pb->adpcm_loop_info.yn2);
      }
      else
      {
        // Refresh YN1 and YN2. This indirectly causes the accelerator to resume reads.
        SetYn1(GetYn1());
        SetYn2(GetYn2());
#ifdef AX_GC
        // If we're streaming, increment the loop counter.
        acc_pb->loop_counter++;
#endif
      }
    }
    else
    {
      // Non looping voice reached the end -> running = 0.
      acc_pb->running = 0;

#ifdef AX_WII
      // One of the few meaningful differences between AXGC and AXWii:
      // while AXGC handles non looping voices ending by relying on the
      // accelerator to stop reads once the loop address is reached,
      // AXWii has the 0000 samples internally in DRAM and use an internal
      // pointer to it (loop addr does not contain 0000 samples on AXWii!).
      acc_end_reached = true;
#endif
    }
  }

  u8 ReadMemory(u32 address) override { return ReadARAM(address); }
  void WriteMemory(u32 address, u8 value) override { WriteARAM(value, address); }
};

static std::unique_ptr<Accelerator> s_accelerator = std::make_unique<HLEAccelerator>();

// Sets up the simulated accelerator.
void AcceleratorSetup(PB_TYPE* pb)
{
  acc_pb = pb;
  s_accelerator->SetStartAddress(HILO_TO_32(pb->audio_addr.loop_addr));
  s_accelerator->SetEndAddress(HILO_TO_32(pb->audio_addr.end_addr));
  s_accelerator->SetCurrentAddress(HILO_TO_32(pb->audio_addr.cur_addr));
  s_accelerator->SetSampleFormat(pb->audio_addr.sample_format);
  s_accelerator->SetYn1(pb->adpcm.yn1);
  s_accelerator->SetYn2(pb->adpcm.yn2);
  s_accelerator->SetPredScale(pb->adpcm.pred_scale);
  acc_end_reached = false;
}

// Reads a sample from the accelerator. Also handles looping and
// disabling streams that reached the end (this is done by an exception raised
// by the accelerator on real hardware).
u16 AcceleratorGetSample()
{
  // See below for explanations about acc_end_reached.
  if (acc_end_reached)
    return 0;

  return s_accelerator->Read(acc_pb->adpcm.coefs);
}

// Reads samples from the input callback, resamples them to <count> samples at
// the wanted sample rate (computed from the ratio, see below).
//
// If srctype is SRCTYPE_POLYPHASE, coefficients need to be provided as well
// (or the srctype will automatically be changed to LINEAR).
//
// Returns the current position after resampling (including fractional part).
//
// The input to output ratio is set in <ratio>, which is a floating point num
// stored as a 32b integer:
//  * Upper 16 bits of the ratio are the integer part
//  * Lower 16 bits are the decimal part
//
// <curr_pos> is a 32b integer structured in the same way as the ratio: the
// upper 16 bits are the integer part of the current position in the input
// stream, and the lower 16 bits are the decimal part.
//
// We start getting samples not from sample 0, but 0.<curr_pos_frac>. This
// avoids discontinuities in the audio stream, especially with very low ratios
// which interpolate a lot of values between two "real" samples.
u32 ResampleAudio(std::function<s16(u32)> input_callback, s16* output, u32 count, s16* last_samples,
                  u32 curr_pos, u32 ratio, int srctype, const s16* coeffs)
{
  int read_samples_count = 0;

  // TODO(delroth): find out why the polyphase resampling algorithm causes
  // audio glitches in Wii games with non integral ratios.

  // If DSP DROM coefficients are available, support polyphase resampling.
  if (0)  // if (coeffs && srctype == SRCTYPE_POLYPHASE)
  {
    s16 temp[4];
    u32 idx = 0;

    temp[idx++ & 3] = last_samples[0];
    temp[idx++ & 3] = last_samples[1];
    temp[idx++ & 3] = last_samples[2];
    temp[idx++ & 3] = last_samples[3];

    for (u32 i = 0; i < count; ++i)
    {
      curr_pos += ratio;
      while (curr_pos >= 0x10000)
      {
        temp[idx++ & 3] = input_callback(read_samples_count++);
        curr_pos -= 0x10000;
      }

      u16 curr_pos_frac = ((curr_pos & 0xFFFF) >> 9) << 2;
      const s16* c = &coeffs[curr_pos_frac];

      s64 t0 = temp[idx++ & 3];
      s64 t1 = temp[idx++ & 3];
      s64 t2 = temp[idx++ & 3];
      s64 t3 = temp[idx++ & 3];

      s64 samp = (t0 * c[0] + t1 * c[1] + t2 * c[2] + t3 * c[3]) >> 15;

      output[i] = (s16)samp;
    }

    last_samples[3] = temp[--idx & 3];
    last_samples[2] = temp[--idx & 3];
    last_samples[1] = temp[--idx & 3];
    last_samples[0] = temp[--idx & 3];
  }
  else if (srctype == SRCTYPE_LINEAR || srctype == SRCTYPE_POLYPHASE)
  {
    // This is the circular buffer containing samples to use for the
    // interpolation. It is initialized with the values from the PB, and it
    // will be stored back to the PB at the end.
    s16 temp[4];
    u32 idx = 0;

    temp[idx++ & 3] = last_samples[0];
    temp[idx++ & 3] = last_samples[1];
    temp[idx++ & 3] = last_samples[2];
    temp[idx++ & 3] = last_samples[3];

    for (u32 i = 0; i < count; ++i)
    {
      curr_pos += ratio;

      // While our current position is >= 1.0, push new samples to the
      // circular buffer.
      while (curr_pos >= 0x10000)
      {
        temp[idx++ & 3] = input_callback(read_samples_count++);
        curr_pos -= 0x10000;
      }

      // Get our current fractional position, used to know how much of
      // curr0 and how much of curr1 the output sample should be.
      u16 curr_frac = curr_pos & 0xFFFF;
      u16 inv_curr_frac = -curr_frac;

      // Interpolate! If curr_frac is 0, we can simply take the last
      // sample without any multiplying.
      s16 sample;
      if (curr_frac)
      {
        s32 s0 = temp[idx++ & 3];
        s32 s1 = temp[idx++ & 3];

        sample = ((s0 * inv_curr_frac) + (s1 * curr_frac)) >> 16;
        idx += 2;
      }
      else
      {
        sample = temp[idx++ & 3];
        idx += 3;
      }

      output[i] = sample;
    }

    // Update the four last_samples values.
    last_samples[3] = temp[--idx & 3];
    last_samples[2] = temp[--idx & 3];
    last_samples[1] = temp[--idx & 3];
    last_samples[0] = temp[--idx & 3];
  }
  else  // SRCTYPE_NEAREST
  {
    // No sample rate conversion here: simply read samples from the
    // accelerator to the output buffer.
    for (u32 i = 0; i < count; ++i)
      output[i] = input_callback(i);

    memcpy(last_samples, output + count - 4, 4 * sizeof(u16));
  }

  return curr_pos;
}

// Read <count> input samples from ARAM, decoding and converting rate
// if required.
void GetInputSamples(PB_TYPE& pb, s16* samples, u16 count, const s16* coeffs)
{
  AcceleratorSetup(&pb);

  if (coeffs)
    coeffs += pb.coef_select * 0x200;
  u32 curr_pos =
      ResampleAudio([](u32) { return AcceleratorGetSample(); }, samples, count, pb.src.last_samples,
                    pb.src.cur_addr_frac, HILO_TO_32(pb.src.ratio), pb.src_type, coeffs);
  pb.src.cur_addr_frac = (curr_pos & 0xFFFF);

  // Update current position, YN1, YN2 and pred scale in the PB.
  pb.audio_addr.cur_addr_hi = static_cast<u16>(s_accelerator->GetCurrentAddress() >> 16);
  pb.audio_addr.cur_addr_lo = static_cast<u16>(s_accelerator->GetCurrentAddress());
  pb.adpcm.yn1 = s_accelerator->GetYn1();
  pb.adpcm.yn2 = s_accelerator->GetYn2();
  pb.adpcm.pred_scale = s_accelerator->GetPredScale();
}

// Add samples to an output buffer, with optional volume ramping.
void MixAdd(int* out, const s16* input, u32 count, u16* pvol, s16* dpop, bool ramp)
{
  u16& volume = pvol[0];
  u16 volume_delta = pvol[1];

  // If volume ramping is disabled, set volume_delta to 0. That way, the
  // mixing loop can avoid testing if volume ramping is enabled at each step,
  // and just add volume_delta.
  if (!ramp)
    volume_delta = 0;

  for (u32 i = 0; i < count; ++i)
  {
    s64 sample = input[i];
    sample *= volume;
    sample >>= 15;
    sample = std::clamp((s32)sample, -32767, 32767);  // -32768 ?

    out[i] += (s16)sample;
    volume += volume_delta;

    *dpop = (s16)sample;
  }
}

// Execute a low pass filter on the samples using one history value. Returns
// the new history value.
s16 LowPassFilter(s16* samples, u32 count, s16 yn1, u16 a0, u16 b0)
{
  for (u32 i = 0; i < count; ++i)
    yn1 = samples[i] = (a0 * (s32)samples[i] + b0 * (s32)yn1) >> 15;
  return yn1;
}

// Process 1ms of audio (for AX GC) or 3ms of audio (for AX Wii) from a PB and
// mix it to the output buffers.
void ProcessVoice(PB_TYPE& pb, const AXBuffers& buffers, u16 count, AXMixControl mctrl,
                  const s16* coeffs)
{
  // If the voice is not running, nothing to do.
  if (!pb.running)
    return;

  // Read input samples, performing sample rate conversion if needed.
  s16 samples[MAX_SAMPLES_PER_FRAME];
  GetInputSamples(pb, samples, count, coeffs);

  // Apply a global volume ramp using the volume envelope parameters.
  for (u32 i = 0; i < count; ++i)
  {
    samples[i] = std::clamp(((s32)samples[i] * pb.vol_env.cur_volume) >> 15, -32767,
                            32767);  // -32768 ?
    pb.vol_env.cur_volume += pb.vol_env.cur_volume_delta;
  }

  // Optionally, execute a low pass filter
  // TODO: LPF code is currently broken, causing Super Monkey Ball sound
  // corruption. Disabled until someone figures out what is wrong with it.
  if (0 && pb.lpf.enabled)
  {
    pb.lpf.yn1 = LowPassFilter(samples, count, pb.lpf.yn1, pb.lpf.a0, pb.lpf.b0);
  }

  // Mix LRS, AUXA and AUXB depending on mixer_control
  // TODO: Handle DPL2 on AUXB.

#define MIX_ON(C) (0 != (mctrl & MIX_##C))
#define RAMP_ON(C) (0 != (mctrl & MIX_##C##_RAMP))

  if (MIX_ON(L))
    MixAdd(buffers.left, samples, count, &pb.mixer.left, &pb.dpop.left, RAMP_ON(L));
  if (MIX_ON(R))
    MixAdd(buffers.right, samples, count, &pb.mixer.right, &pb.dpop.right, RAMP_ON(R));
  if (MIX_ON(S))
    MixAdd(buffers.surround, samples, count, &pb.mixer.surround, &pb.dpop.surround, RAMP_ON(S));

  if (MIX_ON(AUXA_L))
    MixAdd(buffers.auxA_left, samples, count, &pb.mixer.auxA_left, &pb.dpop.auxA_left,
           RAMP_ON(AUXA_L));
  if (MIX_ON(AUXA_R))
    MixAdd(buffers.auxA_right, samples, count, &pb.mixer.auxA_right, &pb.dpop.auxA_right,
           RAMP_ON(AUXA_R));
  if (MIX_ON(AUXA_S))
    MixAdd(buffers.auxA_surround, samples, count, &pb.mixer.auxA_surround, &pb.dpop.auxA_surround,
           RAMP_ON(AUXA_S));

  if (MIX_ON(AUXB_L))
    MixAdd(buffers.auxB_left, samples, count, &pb.mixer.auxB_left, &pb.dpop.auxB_left,
           RAMP_ON(AUXB_L));
  if (MIX_ON(AUXB_R))
    MixAdd(buffers.auxB_right, samples, count, &pb.mixer.auxB_right, &pb.dpop.auxB_right,
           RAMP_ON(AUXB_R));
  if (MIX_ON(AUXB_S))
    MixAdd(buffers.auxB_surround, samples, count, &pb.mixer.auxB_surround, &pb.dpop.auxB_surround,
           RAMP_ON(AUXB_S));

#ifdef AX_WII
  if (MIX_ON(AUXC_L))
    MixAdd(buffers.auxC_left, samples, count, &pb.mixer.auxC_left, &pb.dpop.auxC_left,
           RAMP_ON(AUXC_L));
  if (MIX_ON(AUXC_R))
    MixAdd(buffers.auxC_right, samples, count, &pb.mixer.auxC_right, &pb.dpop.auxC_right,
           RAMP_ON(AUXC_R));
  if (MIX_ON(AUXC_S))
    MixAdd(buffers.auxC_surround, samples, count, &pb.mixer.auxC_surround, &pb.dpop.auxC_surround,
           RAMP_ON(AUXC_S));
#endif

#undef MIX_ON
#undef RAMP_ON

  // Optionally, phase shift left or right channel to simulate 3D sound.
  if (pb.initial_time_delay.on)
  {
    // TODO
  }

#ifdef AX_WII
  // Wiimote mixing.
  if (pb.remote)
  {
    // Old AXWii versions process ms per ms.
    u16 wm_count = count == 96 ? 18 : 6;

    // Interpolate at most 18 samples from the 96 samples we read before.
    s16 wm_samples[18];

    // We use ratio 0x55555 == (5 * 65536 + 21845) / 65536 == 5.3333 which
    // is the nearest we can get to 96/18
    u32 curr_pos = ResampleAudio([&samples](u32 i) { return samples[i]; }, wm_samples, wm_count,
                                 pb.remote_src.last_samples, pb.remote_src.cur_addr_frac, 0x55555,
                                 SRCTYPE_POLYPHASE, coeffs);
    pb.remote_src.cur_addr_frac = curr_pos & 0xFFFF;

// Mix to main[0-3] and aux[0-3]
#define WMCHAN_MIX_ON(n) (0 != ((pb.remote_mixer_control >> (2 * n)) & 3))
#define WMCHAN_MIX_RAMP(n) (0 != ((pb.remote_mixer_control >> (2 * n)) & 2))

    if (WMCHAN_MIX_ON(0))
      MixAdd(buffers.wm_main0, wm_samples, wm_count, &pb.remote_mixer.main0, &pb.remote_dpop.main0,
             WMCHAN_MIX_RAMP(0));
    if (WMCHAN_MIX_ON(1))
      MixAdd(buffers.wm_aux0, wm_samples, wm_count, &pb.remote_mixer.aux0, &pb.remote_dpop.aux0,
             WMCHAN_MIX_RAMP(1));
    if (WMCHAN_MIX_ON(2))
      MixAdd(buffers.wm_main1, wm_samples, wm_count, &pb.remote_mixer.main1, &pb.remote_dpop.main1,
             WMCHAN_MIX_RAMP(2));
    if (WMCHAN_MIX_ON(3))
      MixAdd(buffers.wm_aux1, wm_samples, wm_count, &pb.remote_mixer.aux1, &pb.remote_dpop.aux1,
             WMCHAN_MIX_RAMP(3));
    if (WMCHAN_MIX_ON(4))
      MixAdd(buffers.wm_main2, wm_samples, wm_count, &pb.remote_mixer.main2, &pb.remote_dpop.main2,
             WMCHAN_MIX_RAMP(4));
    if (WMCHAN_MIX_ON(5))
      MixAdd(buffers.wm_aux2, wm_samples, wm_count, &pb.remote_mixer.aux2, &pb.remote_dpop.aux2,
             WMCHAN_MIX_RAMP(5));
    if (WMCHAN_MIX_ON(6))
      MixAdd(buffers.wm_main3, wm_samples, wm_count, &pb.remote_mixer.main3, &pb.remote_dpop.main3,
             WMCHAN_MIX_RAMP(6));
    if (WMCHAN_MIX_ON(7))
      MixAdd(buffers.wm_aux3, wm_samples, wm_count, &pb.remote_mixer.aux3, &pb.remote_dpop.aux3,
             WMCHAN_MIX_RAMP(7));
  }
#undef WMCHAN_MIX_RAMP
#undef WMCHAN_MIX_ON
#endif
}

}  // namespace
}  // namespace HLE
}  // namespace DSP
