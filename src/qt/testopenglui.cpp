#include "testopenglui.h"
#include <QOpenGLTexture>

TestOpenglUI::~TestOpenglUI()
{}

void TestOpenglUI::initializeGL()
{
    setFixedWidth(160);
    setFixedHeight(160);
    initializeOpenGLFunctions();

    glClearColor(0, 0, 0, 0);

//     initShaders();
//     initTextures();

//     // geometries = new GeometryEngine;

//     // Use QBasicTimer because its faster than QTimer
//     // timer.start(12, this);
}

// void TestOpenglUI::initShaders()
// {
// }

// void TestOpenglUI::initTextures()
// {}

void TestOpenglUI::resizeGL(int w, int h)
{
    glViewport(0, 0, 16, 16);
}

void TestOpenglUI::renderBlock()
{
    QImage img1("/tmp/assets/minecraft/textures/block/andesite.png");

    auto tex1 = new QOpenGLTexture(img1);
    
    
}