#include "Tool.h"
#include "ui_Tool.h"

#include <QDebug>
#include <QDir>
#include <QStandardPaths>
#include <QImageWriter>
#include <QKeyEvent>

QString Tool::savePath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);

Tool::Tool(QWidget *parent): QWidget(parent), ui(new Ui::Tool), m_shape(nullptr), m_ignore{false} {
    ui->setupUi(this);
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_X11NetWmWindowTypeDesktop);
    connect(ui->pen_color,  SIGNAL(clicked()),         this, SLOT(penChange()));
    connect(ui->pen_size,   SIGNAL(valueChanged(int)), this, SLOT(penChange(int)));
    connect(ui->btn_save,   SIGNAL(clicked()),         this, SLOT(choosePath()));
    connect(ui->btn_cancel, SIGNAL(clicked()),         this, SIGNAL(cancel()));
    connect(ui->btn_ok,     SIGNAL(clicked()),         this, SIGNAL(save()));
    connect(ui->btn_top,    SIGNAL(clicked()),         this, SIGNAL(clickTop()));
    connect(ui->btn_undo,    SIGNAL(clicked()),        this, SIGNAL(undo()));
    connect(ui->btn_redo,    SIGNAL(clicked()),        this, SIGNAL(redo()));

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
    connect(ui->btn_text, &QPushButton::clicked, this, [=](){
        setDraw(! ui->btn_text->isFlat() ? ShapeEnum::Text : ShapeEnum::Null);
    });

    connect(ui->bold, &QPushButton::clicked, this, [=]() {
        if (ui->bold->isFlat()) {
            ui->bold->setFlat(false);
            m_font.setBold(false);
        } else {
            ui->bold->setFlat(true);
            m_font.setBold(true);
        }
    });
    connect(ui->italic, &QPushButton::clicked, this, [=]() {
        if (ui->italic->isFlat()) {
            ui->italic->setFlat(false);
            m_font.setItalic(false);
        } else {
            ui->italic->setFlat(true);
            m_font.setItalic(true);
        }
    });
    connect(ui->underline, &QPushButton::clicked, this, [=]() {
        if (ui->underline->isFlat()) {
            ui->underline->setFlat(false);
            m_font.setUnderline(false);
        } else {
            ui->underline->setFlat(true);
            m_font.setUnderline(true);
        }
    });
    connect(ui->strikeout, &QPushButton::clicked, this, [=]() {
        if (ui->strikeout->isFlat()) {
            ui->strikeout->setFlat(false);
            m_font.setStrikeOut(false);
        } else {
            ui->strikeout->setFlat(true);
            m_font.setStrikeOut(true);
        }
    });

    setDraw(ShapeEnum::Null);

    m_pen.setWidth(ui->pen_size->value());
    int index = ui->pen_color->styleSheet().indexOf("#");
    if (index != -1 && ui->pen_color->styleSheet().length() >= index + 7) {
        m_pen.setColor(ui->pen_color->styleSheet().mid(index, 7));
    } else {
        m_pen.setColor(Qt::black);
    }

    ui->btn_rectangle->installEventFilter(this);
    ui->btn_ellipse->installEventFilter(this);
    ui->btn_straightline->installEventFilter(this);
    ui->btn_line->installEventFilter(this);
    ui->btn_arrow->installEventFilter(this);
    ui->btn_text->installEventFilter(this);
    ui->btn_save->installEventFilter(this);
    ui->pen_size->installEventFilter(this);
    ui->pen_color->installEventFilter(this);
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
        if (ui->btn_text->isFlat()) {
            return new Text(point, m_pen, m_font, ui->transparency->value(), ui->fill->isChecked());
        } else {
            return m_shape->getInstance(point, m_pen, ui->transparency->value(), ui->fill->isChecked());
        }
    }
}

void Tool::showEvent(QShowEvent *event) {
    if (m_shape != nullptr) {
        setDraw(ShapeEnum::Null);
    }
    QWidget::showEvent(event);
}

void Tool::hideEvent(QHideEvent *event) {
    if (m_shape != nullptr) {
        setDraw(ShapeEnum::Null);
    }
    QWidget::hideEvent(event);
}

bool Tool::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::FocusIn && watched != ui->pen_size) {
        setFocus();
        return true;
    } else if (event->type() == QEvent::FocusOut && watched == ui->pen_size) {
        if (QApplication::focusObject() == nullptr && ! m_ignore) {
            this->setDraw(ShapeEnum::Null);
            this->hide();
        }
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

void Tool::focusOutEvent(QFocusEvent *event) {
    Q_UNUSED(event);
    if (QApplication::focusObject() == nullptr && ! m_ignore) {
        this->setDraw(ShapeEnum::Null);
        this->hide();
    }
}

void Tool::keyPressEvent(QKeyEvent *event) {
    if (event->modifiers() == Qt::ControlModifier) {
        if (event->key() == Qt::Key_S) {
            emit save();
        } else if (event->key() == Qt::Key_Z) {
            emit undo();
        } else if (event->key() == Qt::Key_Y) {
            emit redo();
        }
    } else if (event->modifiers() == Qt::NoModifier) {
        if (event->key() == Qt::Key_Escape) {
            emit cancel();
        }
    }
}

void Tool::setEditShow(bool show) {
    if (show) {
        this->setMinimumWidth(312);
        this->setMaximumWidth(312);
        ui->btn_rectangle->setVisible(true);
        ui->btn_ellipse->setVisible(true);
        ui->btn_straightline->setVisible(true);
        ui->btn_line->setVisible(true);
        ui->btn_arrow->setVisible(true);
        ui->btn_text->setVisible(true);
        ui->btn_top->setVisible(true);
        ui->btn_undo->setVisible(true);
        ui->btn_redo->setVisible(true);
        ui->btn_save->setVisible(true);
    } else {
        if (m_shape != nullptr) {
            setDraw(ShapeEnum::Null);
        }
        ui->btn_rectangle->setVisible(false);
        ui->btn_ellipse->setVisible(false);
        ui->btn_straightline->setVisible(false);
        ui->btn_line->setVisible(false);
        ui->btn_arrow->setVisible(false);
        ui->btn_text->setVisible(false);
        ui->btn_top->setVisible(false);
        ui->btn_undo->setVisible(false);
        ui->btn_redo->setVisible(false);
        ui->btn_save->setVisible(false);
        this->setMinimumWidth(52);
        this->setMaximumWidth(52);
    }
}

void Tool::choosePath() {
    const QString format = "png";
    if (savePath.isEmpty())
        savePath = QDir::currentPath();

    QFileDialog fileDialog(this, tr("选择文件"), savePath);
    fileDialog.setAcceptMode(QFileDialog::AcceptSave);
    fileDialog.setFileMode(QFileDialog::AnyFile);
    fileDialog.setDirectory(savePath);
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
        savePath = fileinfo.absolutePath();
        QString suffix = fileinfo.suffix();
        if (! QImageWriter::supportedImageFormats().contains(suffix.toUtf8())) {
            path += "." + extensions.value(0);
        }
        emit save(path);
    }
}

void Tool::penChange(int value) {
    if (value == -1) {
        m_ignore = true;
        QColor color = QColorDialog::getColor(m_pen.color(), this, "请选择画笔颜色");
        m_ignore = false;
        setFocus();
        if (color.isValid()) {
            m_pen.setColor(color);
            ui->pen_color->setStyleSheet(QString("QPushButton { background-color: %1; }").arg(color.name()));
            emit penChanged(m_pen, true);
        }
    } else {
        m_pen.setWidth(value);
        emit penChanged(m_pen, true);
    }
}

void Tool::setDraw(ShapeEnum shape) {
    QString s = QString("%1,%2,%3,%4").arg(m_pen.width()).arg(ui->fill->isChecked()).arg(m_pen.color().name()).arg(ui->transparency->value());
    if (ui->btn_rectangle->isFlat()) {
        ui->btn_rectangle->setStatusTip(s);
    } else if (ui->btn_ellipse->isFlat()) {
        ui->btn_ellipse->setStatusTip(s);
    } else if (ui->btn_straightline->isFlat()) {
        ui->btn_straightline->setStatusTip(s);
    } else if (ui->btn_line->isFlat()) {
        ui->btn_line->setStatusTip(s);
    } else if (ui->btn_arrow->isFlat()) {
        ui->btn_arrow->setStatusTip(s);
    } else if (ui->btn_text->isFlat()) {
        ui->btn_text->setStatusTip(s);
    }

    ui->btn_rectangle->setFlat(false);
    ui->btn_ellipse->setFlat(false);
    ui->btn_straightline->setFlat(false);
    ui->btn_line->setFlat(false);
    ui->btn_arrow->setFlat(false);
    ui->btn_text->setFlat(false);
    if (shape != ShapeEnum::Null) {
        ui->pen_widget->setVisible(true);
        ui->fill->setVisible(true);
        setMinimumHeight(52);
        setMaximumHeight(52);
        if (shape == ShapeEnum::Text) {
            ui->bold->setVisible(true);
            ui->italic->setVisible(true);
            ui->underline->setVisible(true);
            ui->strikeout->setVisible(true);
        } else {
            ui->bold->setVisible(false);
            ui->italic->setVisible(false);
            ui->underline->setVisible(false);
            ui->strikeout->setVisible(false);
        }
    }
    if (m_shape != nullptr) {
        delete m_shape;
        m_shape = nullptr;
    }
    switch (shape) {
    case ShapeEnum::Rectangle:
        s = ui->btn_rectangle->statusTip();
        ui->btn_rectangle->setFlat(true);
        m_shape = new Rectangle({}, {});
        break;
    case ShapeEnum::Ellipse:
        s = ui->btn_ellipse->statusTip();
        ui->btn_ellipse->setFlat(true);
        m_shape = new Ellipse({}, {});
        break;
    case ShapeEnum::StraightLine:
        ui->fill->setVisible(false);
        s = ui->btn_straightline->statusTip();
        ui->btn_straightline->setFlat(true);
        m_shape = new StraightLine({}, {});
        break;
    case ShapeEnum::Line:
        s = ui->btn_line->statusTip();
        ui->btn_line->setFlat(true);
        m_shape = new Line({}, {});
        break;
    case ShapeEnum::Arrow:
        s = ui->btn_arrow->statusTip();
        ui->btn_arrow->setFlat(true);
        m_shape = new Arrow({}, {});
        break;
    case ShapeEnum::Text:
        ui->fill->setVisible(false);
        s = ui->btn_text->statusTip();
        ui->btn_text->setFlat(true);
        m_shape = new Text({}, {});
        break;
    default:
        s = "";
        ui->pen_widget->setVisible(false);
        setMinimumHeight(26);
        setMaximumHeight(26);
        break;
    }

    QStringList list = s.split(",");
    if (list.size() == 4) {
        ui->pen_size->setValue(list[0].toInt());
        ui->fill->setChecked(list[1].toInt());
        QColor color{list[2]};
        ui->pen_color->setStyleSheet(QString("QPushButton { background-color: %1; }").arg(color.name()));
        m_pen.setColor(color);
        ui->transparency->setValue(list[3].toFloat());
    }
}

void Tool::topChange() {
    bool top = ! ui->btn_top->isFlat();
    ui->btn_top->setFlat(top);
    ui->btn_top->setToolTip(top ? "取消置顶" : "置顶");
    emit topChanged(top);
}
