#pragma once
#include <tuple>
#include <Gamma/Envelope.h>
#include <Gamma/Filter.h>
#include <Gamma/Oscillator.h>
#include <Gamma/Noise.h>

#include "core/voices/voice_manager.hpp"
#include "ottofm.hpp"

namespace otto::engines::ottofm {

  /// Custom version of the 'Sine' in Gamma. We need to call it with a phase offset
  /// instead of a frequency offset. (Phase modulation, not frequency modulation)
  /// Defines its own action handlers, which is why it is templated.
  template<int I>
  struct FMOperator {
  
    struct FMSine : public gam::AccumPhase<> {
      FMSine(float frq = 440, float phs = 0) : AccumPhase<>(frq, phs, 1) {}
      /// Generate next sample with phase offset
      float operator()(float phsOffset) noexcept 
      {
        return gam::scl::sinP9(gam::scl::wrap(this->nextPhase() + phsOffset, 1.f, -1.f));
      };
    };

    FMOperator(float frq = 440, float outlevel = 1, bool modulator = false) {}

    float operator()(float phaseMod = 0) noexcept 
    {
      if (modulator_)
        return env_() * sine(phaseMod) * outlevel_ * fm_amount_;
      else {
        previous_value_ = sine(phaseMod + feedback_ * previous_value_) * outlevel_;
        return previous_value_;
      }
    };

    /// Set frequency
    void freq(float frq)
    {
      sine.freq(frq);
    }; 

    /// Get current level
    float level() { return env_.value() * outlevel_; }; 

    /// Set fm_amount
    void fm_amount(float fm){ fm_amount_ = fm; };

    /// Reset envelope
    void reset(){ env_.resetSoft(); };

    /// Release envelope
    void release(){ env_.release(); };

    /// Finish envelope
    void finish(){ env_.finish(); };

    /// Set modulator flag
    void modulator(bool m){ modulator_ = m; };

    /// Actionhandlers. These are all the properties that can vary across operators.
    void action(itc::prop_change<&Props::OperatorProps<I>::feedback>, float f)
    {
      feedback_ = f;
    };
    void action(itc::prop_change<&Props::OperatorProps<I>::mAtt>, float a)
    {
      env_.attack(3 * a);
    };
    void action(itc::prop_change<&Props::OperatorProps<I>::mSuspos>, float s)
    {
      suspos_ = s;
      env_.decay(3 * decrel_ * (1 - s));
      env_.release(3 * decrel_ * s);
      env_.sustain(s);
    };
    void action(itc::prop_change<&Props::OperatorProps<I>::mDecrel>, float dr)
    {
      decrel_ = dr;
      env_.decay(3 * dr * (1 - suspos_));
      env_.release(3 * dr * suspos_);
    };
    void action(itc::prop_change<&Props::OperatorProps<I>::detune>, float d)
    {
      detune_amount_ = d * 25;
    };
    void action(itc::prop_change<&Props::OperatorProps<I>::ratio_idx>, int idx)
    {
      freq_ratio_ = (float) fractions[idx];
    };
    void action(itc::prop_change<&Props::OperatorProps<I>::outLev>, float l)
    {
      outlevel_ = l;
    };


  private:
    FMSine sine;
    gam::ADSR<> env_;

    bool modulator_ = false; /// If it is a modulator, use the envelope.
    float outlevel_ = 1;
    float feedback_ = 0;
    float fm_amount_ = 1;

    // Necessary for housekeeping under envelope property changes
    float suspos_ = 1;
    float decrel_ = 1;

    float freq_ratio_ = 1;
    float detune_amount_ = 0;

    float previous_value_ = 0;
  };

  struct Voice : voices::VoiceBase<Voice> {
    Voice(Audio& a) noexcept;

    // Not private because we need to show activity levels on screen
    std::tuple<FMOperator<0>, FMOperator<1>, FMOperator<2>, FMOperator<3>> operators;
    gam::ADSR<> env_ = {0.1f, 0.1f, 0.7f, 2.0f, 1.f, -4.f};

    // These voices only have process calls.
    // This saves us from checking the current algorithm every sample.
    core::audio::ProcessData<1> process(core::audio::ProcessData<1> data) noexcept;

    void on_note_on(float) noexcept;
    void on_note_off() noexcept;

    void reset_envelopes() noexcept;
    void release_envelopes() noexcept;

    void set_frequencies() noexcept;

    /// Use actions from base class
    using VoiceBase::action;

    // Here are declarations for actionhandlers for the voice.
    // It's all of the properties not being handled by individual operators.
    // While they could be placed in Audio, the suggested style is to keep
    // actionhandlers in Audio to Pre- and Post-processing, not properties affecting the voices.
    void action(itc::prop_change<&Props::fmAmount>, float fm) noexcept
    {
      util::tuple_for_each(operators, [fm](auto& op){ op.fm_amount(fm);} );
    };
    void action(itc::prop_change<&Props::algN>, int a) noexcept
    {
      // Raw loop, sorry. Is there a better way?
      for (int i=0; i<std::tuple_size<decltype(operators)>::value; i++)
      {
        std::get<i>(operators).modulator( algorithms[a].modulator_flags[i]; );
      }
    };

    void action(voices::attack_tag::action, float a) noexcept
    {
      env_.attack(a * a * 8.f + 0.005f);
    }
    void action(voices::decay_tag::action, float d) noexcept
    {
      env_.decay(d * d * 4.f + 0.005f);
    }
    void action(voices::sustain_tag::action, float s) noexcept
    {
      env_.sustain(s);
    }
    void action(voices::release_tag::action, float r) noexcept
    {
      env_.release(r * r * 8.f + 0.005f);
    }

  private:
    Audio& audio;
  };

  struct Audio {
    Audio() noexcept;

    float get_activity_level(int op) noexcept;

    /// Passes unhandled actions to voices
    template<typename Tag, typename... Args>
    auto action(itc::Action<Tag, Args...> a, Args... args) noexcept
      -> std::enable_if_t<itc::ActionReciever::is<voices::VoiceManager<Voice, 6>, itc::Action<Tag, Args...>>>
    {
      voice_mgr_.action(a, args...);
    }

    // Only a process call, since this sums the process calls of the voices.
    audio::ProcessData<1> process(audio::ProcessData<1>) noexcept;

  private:
    friend Voice;

    int algN_ = 0;
    int cur_op_ = 0;

    // Only used internally. At the end of the buffer, 
    // update activity values of operators from last triggered voice
    Voice* last_voice = nullptr;

    std::atomic<float>* shared_activity0 = nullptr;
    std::atomic<float>* shared_activity1 = nullptr;
    std::atomic<float>* shared_activity2 = nullptr;
    std::atomic<float>* shared_activity3 = nullptr;

    voices::VoiceManager<Voice, 6> voice_mgr_ = {*this};
  };

}