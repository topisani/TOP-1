#include "engine_selector_screen.hpp"

#include "core/ui/vector_graphics.hpp"
#include "services/preset_manager.hpp"

namespace otto::engines {

  using namespace otto::ui;
  using namespace otto::ui::vg;

  SelectorWidget::Options EngineSelectorScreen::eng_opts(
    std::function<AnyEngine&(int)>&& select_eg) noexcept
  {
        SelectorWidget::Options opts;
        opts.on_select = [this, sl = std::move(select_eg)](int idx) {
          auto& eg = sl(idx);
          preset_wid.items(presets::preset_names(eg.name()));
          preset_wid.select(eg.current_preset(), true);
        };
        opts.item_colour = Colours::Blue;
        opts.size = {120, vg::HEIGHT};
        return opts;
  }

  SelectorWidget::Options EngineSelectorScreen::prst_opts(
    std::function<AnyEngine&()>&& cur_eg) noexcept
  {
        SelectorWidget::Options opts;
        opts.on_select = [cur_eg = std::move(cur_eg)](int idx) {
            presets::apply_preset(cur_eg(), idx);
          };
        opts.item_colour = Colours::Green;
        opts.size = {120, vg::HEIGHT};
        return opts;
  }

  void EngineSelectorScreen::rotary(RotaryEvent e)
  {
    switch (e.rotary) {
    case Rotary::Blue:
      engine_wid.prev(e.clicks);
      break;
    case Rotary::Green:
      preset_wid.prev(e.clicks);
      break;
    default:;
    }
  }

  void EngineSelectorScreen::draw(vg::Canvas& ctx)
  {
    ctx.drawAt({40, 0}, engine_wid);
    ctx.drawAt({160, 0}, preset_wid);
  }

  void EngineSelectorScreen::on_show()
  {
    engine_wid.options.on_select(engine_wid.selected_item());
  }

  void EngineSelectorScreen::on_hide()
  {}

} // namespace otto::engines
