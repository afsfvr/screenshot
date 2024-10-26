#include "Tool.h"
#include "ui_Tool.h"

#include <QDebug>
#include <QDir>

Tool::Tool(QWidget *parent): QWidget(parent), ui(new Ui::Tool), m_pen(Qt::black, 2), m_shape(nullptr) {
    ui->setupUi(this);
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    connect(ui->pen_color,  SIGNAL(clicked()),         this, SLOT(penChange()));
    connect(ui->pen_size,   SIGNAL(valueChanged(int)), this, SLOT(penChange(int)));
    connect(ui->btn_save,   SIGNAL(clicked()),         this, SLOT(choosePath()));
    connect(ui->btn_cancel, SIGNAL(clicked()),         this, SIGNAL(cancel()));
    connect(ui->btn_ok,     SIGNAL(clicked()),         this, SIGNAL(save()));
    connect(ui->btn_top,    SIGNAL(clicked()),         this, SIGNAL(clickTop()));

    connect(ui->btn_rectangle, &QPushButton::clicked, this, [=](){
        ui->btn_rectangle->setFlat(! ui->btn_rectangle->isFlat());
        setDraw(ui->btn_rectangle->isFlat() ? ShapeEnum::Rectangle : ShapeEnum::Null);
    });
    connect(ui->btn_ellipse, &QPushButton::clicked, this, [=](){
        ui->btn_ellipse->setFlat(! ui->btn_ellipse->isFlat());
        setDraw(ui->btn_ellipse->isFlat() ? ShapeEnum::Ellipse : ShapeEnum::Null);
    });
    connect(ui->btn_line, &QPushButton::clicked, this, [=](){
        ui->btn_line->setFlat(! ui->btn_line->isFlat());
        setDraw(ui->btn_line->isFlat() ? ShapeEnum::Line : ShapeEnum::Null);
    });
    connect(ui->btn_straightline, &QPushButton::clicked, this, [=](){
        ui->btn_straightline->setFlat(! ui->btn_straightline->isFlat());
        setDraw(ui->btn_straightline->isFlat() ? ShapeEnum::StraightLine : ShapeEnum::Null);
    });

    setDraw(ShapeEnum::Null);
}

Tool::~Tool()
{
    delete ui;
}

bool Tool::isDraw() {
    return m_shape != nullptr;
}

Shape *Tool::getShape(const QPoint &point) {
    if (m_shape == nullptr) {
        return nullptr;
    } else {
        return m_shape->getInstance(point, m_pen);
    }
}

void Tool::showEvent(QShowEvent *event) {
    Q_UNUSED(event);
    setDraw(ShapeEnum::Null);
}

void Tool::choosePath() {
    QString path = QFileDialog::getSaveFileName(this, "选择文件", QDir::homePath(), "Images(*.png, *.jpg, *.jpeg)");
    if (path.length() != 0) {
        QString suffix = QFileInfo(path).suffix();
        if (suffix != "png" && suffix != "jpg" && suffix != "jpeg") {
            path += ".png";
            emit save(path);
        }
    }
}

void Tool::penChange(int value) {
    if (value == -1) {
        QColor color = QColorDialog::getColor(m_pen.color(), this);
        if (color.isValid()) {
            m_pen.setColor(color);
            ui->pen_color->setStyleSheet(QString("background-color: %1;").arg(color.name()));
            emit penChanged(m_pen, true);
        }
    } else {
        m_pen.setWidth(value);
        emit penChanged(m_pen, true);
    }
}

void Tool::setDraw(ShapeEnum shape) {
    ui->btn_rectangle->setFlat(false);
    ui->btn_ellipse->setFlat(false);
    ui->btn_straightline->setFlat(false);
    ui->btn_line->setFlat(false);
    if (shape != ShapeEnum::Null) {
        ui->pen_widget->setVisible(true);
        setMinimumHeight(52);
        setMaximumHeight(52);
    }
    if (m_shape != nullptr) {
        delete m_shape;
        m_shape = nullptr;
    }
    switch (shape) {
    case ShapeEnum::Rectangle:
        ui->btn_rectangle->setFlat(true);
        ui->pen_widget->move(ui->btn_rectangle->x(), 26);
        m_shape = new Rectangle({}, {});
        break;
    case ShapeEnum::Ellipse:
        ui->btn_ellipse->setFlat(true);
        ui->pen_widget->move(ui->btn_ellipse->x(), 26);
        m_shape = new Ellipse({}, {});
        break;
    case ShapeEnum::StraightLine:
        ui->btn_straightline->setFlat(true);
        ui->pen_widget->move(ui->btn_straightline->x(), 26);
        m_shape = new StraightLine({}, {});
        break;
    case ShapeEnum::Line:
        ui->btn_line->setFlat(true);
        ui->pen_widget->move(ui->btn_line->x(), 26);
        m_shape = new Line({}, {});
        break;
    default:
        ui->pen_widget->setVisible(false);
        setMinimumHeight(26);
        setMaximumHeight(26);
        break;
    }
}

void Tool::topChange() {
    bool top = ! ui->btn_top->isFlat();
    ui->btn_top->setFlat(top);
    emit topChanged(top);
}
