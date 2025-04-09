#ifndef TESTOPENGLUI_H
#define TESTOPENGLUI_H

#include <QtOpenGLWidgets/QOpenGLWidget>
#include <QOpenGLFunctions>

class TestOpenglUI : public QOpenGLWidget , protected QOpenGLFunctions
{
    Q_OBJECT

public:
    using QOpenGLWidget::QOpenGLWidget;
    ~TestOpenglUI();

protected:
    // void mousePressEvent(QMouseEvent *e) override;
    // void mouseReleaseEvent(QMouseEvent *e) override;
    // void timerEvent(QTimerEvent *e) override;

    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    // void initShaders();
    // void initTextures();

private:
    void renderBlock();
};

#endif /* TESTOPENGLUI_H */