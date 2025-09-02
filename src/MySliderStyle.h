#ifndef MYSLIDERSTYLE_H
#define MYSLIDERSTYLE_H

#include <QObject>
#include <QProxyStyle>

class MySliderStyle : public QProxyStyle {
    Q_OBJECT
public:
    using QProxyStyle::QProxyStyle;

    int styleHint(StyleHint hint, const QStyleOption *option = nullptr, const QWidget *widget = nullptr, QStyleHintReturn *returnData = nullptr) const override;
};

#endif // MYSLIDERSTYLE_H
