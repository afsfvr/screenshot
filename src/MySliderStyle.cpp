#include "MySliderStyle.h"

int MySliderStyle::styleHint(StyleHint hint, const QStyleOption *option, const QWidget *widget, QStyleHintReturn *returnData) const {
    if (hint == StyleHint::SH_Slider_AbsoluteSetButtons) {
        return Qt::LeftButton;
    }
    return QProxyStyle::styleHint(hint, option, widget, returnData);
}
