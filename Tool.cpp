#include "Tool.h"
#include "ui_Tool.h"

#include <QDebug>
#include <QDir>
#include <QStandardPaths>
#include <QImageWriter>

Tool::Tool(QWidget *parent): QWidget(parent), ui(new Ui::Tool), m_shape(nullptr) {
    ui->setupUi(this);
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    connect(ui->pen_color,  SIGNAL(clicked()),         this, SLOT(penChange()));
    connect(ui->pen_size,   SIGNAL(valueChanged(int)), this, SLOT(penChange(int)));
    connect(ui->btn_save,   SIGNAL(clicked()),         this, SLOT(choosePath()));
    connect(ui->btn_cancel, SIGNAL(clicked()),         this, SIGNAL(cancel()));
    connect(ui->btn_ok,     SIGNAL(clicked()),         this, SIGNAL(save()));
    connect(ui->btn_top,    SIGNAL(clicked()),         this, SIGNAL(clickTop()));

    connect(ui->btn_rectangle, &QPushButton::clicked, this, [=](){
        setDraw(! ui->btn_rectangle->isFlat() ? ShapeEnum::Rectangle : ShapeEnum::Null);
    });
    connect(ui->btn_ellipse, &QPushButton::clicked, this, [=](){
        setDraw(! ui->btn_ellipse->isFlat() ? ShapeEnum::Ellipse : ShapeEnum::Null);
    });
    connect(ui->btn_straightline, &QPushButton::clicked, this, [=](){
        setDraw(! ui->btn_straightline->isFlat() ? ShapeEnum::StraightLine : ShapeEnum::Null);
    });
    connect(ui->btn_line, &QPushButton::clicked, this, [=](){
        setDraw(! ui->btn_line->isFlat() ? ShapeEnum::Line : ShapeEnum::Null);
    });
    connect(ui->btn_arrow, &QPushButton::clicked, this, [=](){
        setDraw(! ui->btn_arrow->isFlat() ? ShapeEnum::Arrow : ShapeEnum::Null);
    });

    setDraw(ShapeEnum::Null);
    m_path = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);

    m_pen.setWidth(ui->pen_size->value());
    int index = ui->pen_color->styleSheet().indexOf("#");
    if (index != -1 && ui->pen_color->styleSheet().length() >= index + 7) {
        m_pen.setColor(ui->pen_color->styleSheet().mid(index, 7));
    } else {
        m_pen.setColor(Qt::black);
    }
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
    setDraw(ShapeEnum::Null);
    QWidget::showEvent(event);
}

void Tool::choosePath() {

    const QString format = "png";
    if (m_path.isEmpty())
        m_path = QDir::currentPath();
    // m_path += tr("/untitled.") + format;

    QFileDialog fileDialog(this, tr("选择文件"), m_path);
    fileDialog.setAcceptMode(QFileDialog::AcceptSave);
    fileDialog.setFileMode(QFileDialog::AnyFile);
    fileDialog.setDirectory(m_path);
    QStringList mimeTypes;
    const QList<QByteArray> baMimeTypes = QImageWriter::supportedMimeTypes();
    for (const QByteArray &bf : baMimeTypes)
        mimeTypes.append(QLatin1String(bf));
    fileDialog.setMimeTypeFilters(mimeTypes);
    fileDialog.setFileMode(QFileDialog::AnyFile);
    fileDialog.selectMimeTypeFilter("image/" + format);
    // fileDialog.setDefaultSuffix(format);
    if (fileDialog.exec() != QDialog::Accepted)
        return;

    const QString &filter = fileDialog.selectedNameFilter();
    QRegExp regex("\\(([^)]+)\\)");
    QStringList extensions;
    if (regex.indexIn(filter) != -1) {
        QString matchedString = regex.cap(1);
        QStringList parts = matchedString.split(QRegExp("\\s+"), Qt::SkipEmptyParts);
        QRegExp alphaRegex("[a-zA-Z]+");
        for (const QString &part : parts) {
            if (alphaRegex.indexIn(part) != -1) {
                extensions.append(alphaRegex.cap(0));
            }
        }
    }
    if (extensions.empty()) {
        extensions.append(format);
    }

    QString path = fileDialog.selectedFiles().value(0);
    if (path.length() != 0) {
        QFileInfo fileinfo{path};
        m_path = fileinfo.absolutePath();
        QString suffix = fileinfo.suffix();
        if (! QImageWriter::supportedImageFormats().contains(suffix.toUtf8())) {
            path += "." + extensions.value(0);
        }
        emit save(path);
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
    ui->btn_arrow->setFlat(false);
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
    case ShapeEnum::Arrow:
        ui->btn_arrow->setFlat(true);
        ui->pen_widget->move(ui->btn_arrow->x(), 26);
        m_shape = new Arrow({}, {});
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
