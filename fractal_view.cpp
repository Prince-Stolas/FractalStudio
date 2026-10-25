#include "fractal_view.h"

void View::drawOrbit(QPainter &painter, QSize size) {
    if (!settings.orbit || !hasPoint)
        return;
    using C = std::complex<double>;
    C pixel(px, py), z(0);
    int iteration = 0;
    QVector<C> past(settings.historySize);
    auto historyValue = [&](int i) {
        if (i < 0) {
            auto value = settings.negativeHistory.value(i);
            return C(value.x(), value.y());
        }
        return past.at(i % settings.historySize);
    };
    auto value = [&](QString name) -> C {
        if (name == "z" || name == "z_n")
            throw std::runtime_error("Use z[index]");
        if (name == "pixel")
            return pixel;
        if (name == "n")
            return C(iteration);
        int index = settings.names.indexOf(name);
        if (index < 0)
            throw std::runtime_error("Unknown variable");
        const auto &v = settings.variables[index];
        return v.source == Pixel ? pixel : C(v.real, v.imag);
    };
    const auto &start = settings.variables[3];
    try {
        z = start.source == Pixel        ? pixel
            : start.source == Expression ? ComplexExpression::evaluate(start.equation, value)
                                         : C(start.real, start.imag);
        auto screen = [&](C point) {
            return QPointF(
                double((point.real() - cx) / span * size.height() + size.width() * .5L),
                double((cy - point.imag()) / span * size.height() + size.height() * .5L));
        };
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(QColor("#ffeb55"), 1.5));
        for (iteration = 0; iteration < settings.orbitSteps; ++iteration) {
            if (iteration == 0)
                past[0] = z;
            C next = ComplexExpression::evaluate(settings.equation, value, historyValue, settings.historySize);
            past[(iteration + 1) % settings.historySize] = next;
            if (!std::isfinite(next.real()) || !std::isfinite(next.imag()))
                break;
            QPointF a = screen(z), b = screen(next);
            if (std::abs(a.x()) < 1e7 && std::abs(a.y()) < 1e7 && std::abs(b.x()) < 1e7 &&
                std::abs(b.y()) < 1e7) {
                painter.drawLine(a, b);
                painter.drawEllipse(b, 2, 2);
            }
            z = next;
            if (std::abs(z) > settings.bailout)
                break;
        }
    } catch (const std::exception &e) {
        if (status)
            status(e.what());
    }
}

QString View::negativeDeclarations() const {
    QString out;
    int i = 0;
    for (auto it = settings.negativeHistory.begin(); it != settings.negativeHistory.end(); ++it)
        out += "uniform dvec2 negative" + QString::number(i++) + ";\n";
    out += "dvec2 negativeAt(double index){";
    i = 0;
    for (auto it = settings.negativeHistory.begin(); it != settings.negativeHistory.end(); ++it)
        out += "if(index==" + QString::number(it.key()) + ".0)return negative" +
               QString::number(i++) + ";";
    return out + "return dvec2(0);}\n";
}

QString View::declarations() const {
    QString out = negativeDeclarations();
    for (int i = 0; i < settings.variables.size(); ++i)
        out += "uniform dvec2 input" + QString::number(i) + ";\n";
    return out;
}

QString View::bindings() const {
    QString out;
    for (int i = 0; i < settings.variables.size(); ++i)
        out += "dvec2 var" + QString::number(i) + "=" +
               (settings.variables[i].source == Pixel ? QString("pixel")
                                                      : "input" + QString::number(i)) +
               ";\n";
    return out;
}

QString View::fragment() const {
    return QString(R"GLSL(#version 400 core
out vec4 frag;
uniform dvec2 center,constantC,paramA,paramB,z0;
%3
 dvec2 past[%5];double currentN;bool invalidHistory=false;
 dvec2 historyAt(dvec2 index){if(index.y!=0.0||isnan(index.x)||isinf(index.x)||floor(index.x)!=index.x){invalidHistory=true;return dvec2(0);}if(index.x<0.0)return negativeAt(index.x);if(index.x<max(0.0,currentN-double(%5-1))||index.x>currentN){invalidHistory=true;return dvec2(0);}return past[clamp(int(mod(index.x,double(%5))),0,%5-1)];}
 dvec2 cmod(dvec2 x,dvec2 y){if(y.y!=0.0||y.x==0.0){invalidHistory=true;return dvec2(0);}return x-y.x*floor(x/y.x);}
uniform double scale,bailout;
uniform vec2 resolution,marker;
uniform int iterations,aSource,bSource,cSource,palette,showMarker,quadratic;
uniform float frequency,phase,smoothing;
uniform sampler1D gradientTexture;
uniform int colourMode, escapeSolid, interiorGradient, convergenceEnabled, trapShape;
uniform vec3 escapeColour, interiorColour;
uniform double tolerance, trapReal, trapImag, trapRadius;
uniform sampler1D interiorTexture;
vec3 gradient(float t){return texture(gradientTexture,clamp(t,0.0,1.0)).rgb;}
dvec2 mul(dvec2 a,dvec2 b){return dvec2(a.x*b.x-a.y*b.y,a.x*b.y+a.y*b.x);}
dvec2 divide(dvec2 a,dvec2 b){return dvec2(dot(a,b),a.y*b.x-a.x*b.y)/dot(b,b);}
// GLSL double precision has no transcendental overloads on core desktop OpenGL.
dvec2 clog(dvec2 z){return dvec2(double(log(float(length(z)))),double(atan(float(z.y),float(z.x))));}
dvec2 cexp(dvec2 z){return double(exp(float(z.x)))*dvec2(cos(float(z.y)),sin(float(z.y)));}
dvec2 cpow(dvec2 z,dvec2 w){
 if(w.y==0.0&&w.x==2.0)return mul(z,z);
 if(w.y==0.0&&w.x==3.0)return mul(mul(z,z),z);
 if(w.y==0.0&&w.x==4.0){dvec2 q=mul(z,z);return mul(q,q);}
 if(dot(z,z)==0.0&&w.y==0.0&&w.x>0.0)return dvec2(0);
 return cexp(mul(w,clog(z)));
}
dvec2 csin(dvec2 z){return dvec2(sin(float(z.x))*cosh(float(z.y)),cos(float(z.x))*sinh(float(z.y)));}
dvec2 ccos(dvec2 z){return dvec2(cos(float(z.x))*cosh(float(z.y)),-sin(float(z.x))*sinh(float(z.y)));}
dvec2 conjugate(dvec2 z){return dvec2(z.x,-z.y);}
dvec2 componentAbs(dvec2 z){return abs(z);}
dvec2 magnitude(dvec2 z){return dvec2(length(z),0);}
dvec2 realPart(dvec2 z){return dvec2(z.x,0);}
dvec2 imagPart(dvec2 z){return dvec2(z.y,0);}
void main(){
 dvec2 pixel=center+dvec2(gl_FragCoord.xy-resolution*.5)*(scale/double(resolution.y));
 dvec2 c=cSource==2?pixel:constantC,a=aSource==2?pixel:paramA,b=bSource==2?pixel:paramB;
 %4
 double n=0.0;currentN=0.0;dvec2 z=%1;past[0]=z;
 int count=0;double radius=0.0;
 bool inside=false;
 if(quadratic!=0){double y2=c.y*c.y,x=c.x-.25,q=x*x+y2;inside=q*(q+x)<=.25*y2||(c.x+1.0)*(c.x+1.0)+y2<=.0625;}
 if(inside)count=iterations;
 bool escaped = false;
 bool converged = false;
 int convergenceCount = 0;
 double trapMinimum = 1e30;
 if(!inside) for(int j=0;j<10000;j++) {
     if(j>=iterations) break;
     n=double(j);
     currentN=n;
     z=%2;
     if(invalidHistory) break;
     bool stable = j >= %5-1;
     if(stable && convergenceEnabled!=0) {
         for(int k=0;k<%5;k++) {
             if(length(z-past[k])>tolerance) stable=false;
         }
     }
     if(convergenceEnabled!=0 && stable) {
         if(!converged) convergenceCount=j+1;
         converged=true;
     } else {
         converged=false;
     }
     past[(j+1) % %5]=z;
     dvec2 offset=z-dvec2(trapReal,trapImag);
     double distance=length(offset);
     if(trapShape==1) distance=abs(distance-trapRadius);
     if(trapShape==2) distance=min(abs(offset.x),abs(offset.y));
     trapMinimum=min(trapMinimum,distance);
     radius=dot(z,z);
     count=j+1;
     escaped=isnan(radius)||isinf(radius)||radius>bailout*bailout;
     if(escaped) break;
 }
 vec3 rgb=interiorColour;
 if(escaped){float t=float(count);if(radius>1.0&&!isinf(radius))t+=smoothing*(1.0-log(max(log(float(radius))*.5,.0001))/log(2.0));
 float v=t*frequency+phase;
 if(palette==0)rgb=.5+.5*cos(6.2831853*(v+vec3(.02,.19,.36)));
 else if(palette==1)rgb=.5+.5*cos(6.2831853*(v+vec3(.0,.08,.20)));
 else if(palette==2)rgb=.5+.5*cos(6.2831853*(v+vec3(.50,.32,.10)));
 else rgb=vec3(.5+.5*cos(6.2831853*v));
 rgb*=.8+.2*(1.0-exp(-max(t,0.0)*.06));
 if(palette>=4)rgb=gradient(1.0-abs(2.0*fract(v*.5)-1.0));
 }
 if(escaped && escapeSolid!=0) rgb=escapeColour;
 if(!escaped && converged && interiorGradient!=0) {
     float v=float(convergenceCount)*frequency+phase;
     rgb=texture(interiorTexture,1.0-abs(2.0*fract(v*.5)-1.0)).rgb;
 }
 if(colourMode==1) {
     float v=float(trapMinimum)*frequency+phase;
     v=1.0-exp(-max(v,0.0));
     if(palette>=4) rgb=gradient(v);
     else if(palette==0) rgb=.5+.5*cos(6.2831853*(v+vec3(.02,.19,.36)));
     else if(palette==1) rgb=.5+.5*cos(6.2831853*(v+vec3(.0,.08,.20)));
     else if(palette==2) rgb=.5+.5*cos(6.2831853*(v+vec3(.50,.32,.10)));
     else rgb=vec3(.5+.5*cos(6.2831853*v));
 }
 if(showMarker!=0){vec2 d=abs(gl_FragCoord.xy-marker);if((d.x<1&&d.y<8)||(d.y<1&&d.x<8))rgb=vec3(1);else if((d.x<2&&d.y<9)||(d.y<2&&d.x<9))rgb=vec3(0);}
 if(invalidHistory)rgb=vec3(1.0,0.0,1.0);
 frag=vec4(rgb,1);
})GLSL")
        .arg(initial, expr, declarations(), bindings()).arg(settings.historySize);
}

bool View::rebuild() {
    auto *candidate = new QOpenGLShaderProgram;
    const char *vertex = "#version 400 core\nvoid main(){vec2 "
                         "p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2.-1.,0,1);}";
    if (!candidate->addShaderFromSourceCode(QOpenGLShader::Vertex, vertex) ||
        !candidate->addShaderFromSourceCode(QOpenGLShader::Fragment, fragment()) ||
        !candidate->link()) {
        lastError = candidate->log();
        delete candidate;
        return false;
    }
    delete shader;
    shader = candidate;
    lastError.clear();
    return true;
}

void View::uniform(const char *name, double x) {
    glUniform1d(shader->uniformLocation(name), x);
}

void View::uniform2(const char *name, double x, double y) {
    glUniform2d(shader->uniformLocation(name), x, y);
}

void View::inspect(QPointF pos, bool link) {
    auto [x, y] = coordinate(pos);
    px = double(x);
    py = double(y);
    hasPoint = true;
    if (status)
        status(QString("%1 %2 %3i    |    span %4")
                   .arg(px, 0, 'g', 16)
                   .arg(py < 0 ? "−" : "+")
                   .arg(std::abs(py), 0, 'g', 16)
                   .arg(double(span), 0, 'g', 5));
    if (link && mousePicked)
        mousePicked(px, py, QGuiApplication::keyboardModifiers());
    update();
}

void View::initializeGL() {
    if (!initializeOpenGLFunctions()) {
        lastError = "OpenGL 4.0 is required";
        return;
    }
    ready = true;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    rebuild();
}

void View::paintGL() {
    glClearColor(.016f, .020f, .034f, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    if (!shader)
        return;
    shader->bind();
    glBindVertexArray(vao);
    float ratio = float(devicePixelRatioF());
    float w = exportSize.isEmpty() ? width() * ratio : exportSize.width(),
          h = exportSize.isEmpty() ? height() * ratio : exportSize.height();
    glViewport(0, 0, int(w), int(h));
    const bool quadratic = expr == "(cpow(z,dvec2(p,0.0))+c)" && initial == "dvec2(0.0,0.0)" &&
                           settings.variables[2].source == Pixel && settings.bailout >= 2;
    shader->setUniformValue("quadratic", int(quadratic && !settings.colouring.convergence && settings.colouring.mode == 0));
    const auto &colouring = settings.colouring;
    shader->setUniformValue("colourMode", colouring.mode);
    shader->setUniformValue("escapeSolid", int(colouring.escapeSolid));
    shader->setUniformValue("interiorGradient", int(colouring.interiorGradient));
    shader->setUniformValue("convergenceEnabled", int(colouring.convergence));
    shader->setUniformValue("trapShape", colouring.trap);
    shader->setUniformValue("escapeColour", QVector3D(colouring.escapeColour.redF(), colouring.escapeColour.greenF(), colouring.escapeColour.blueF()));
    shader->setUniformValue("interiorColour", QVector3D(colouring.interiorColour.redF(), colouring.interiorColour.greenF(), colouring.interiorColour.blueF()));
    uniform("tolerance", colouring.tolerance);
    uniform("trapReal", colouring.trapReal);
    uniform("trapImag", colouring.trapImag);
    uniform("trapRadius", colouring.trapRadius);
    uniform2("center", double(cx), double(cy));
    uniform("scale", double(span));
    uniform("bailout", settings.bailout);
    int negativeIndex = 0;
    for (auto it = settings.negativeHistory.begin(); it != settings.negativeHistory.end(); ++it)
        uniform2(qPrintable(QString("negative%1").arg(negativeIndex++)), it.value().x(),
                 it.value().y());
    const char *names[] = {"paramA", "paramB", "constantC", "z0"};
    for (int i = 0; i < 4; ++i)
        uniform2(names[i], settings.variables[i].real, settings.variables[i].imag);
    for (int i = 0; i < settings.variables.size(); ++i)
        uniform2(("input" + QString::number(i)).toLatin1().constData(), settings.variables[i].real,
                 settings.variables[i].imag);
    shader->setUniformValue("aSource", settings.variables[0].source);
    shader->setUniformValue("bSource", settings.variables[1].source);
    shader->setUniformValue("resolution", QVector2D(w, h));
    shader->setUniformValue("iterations", settings.iterations);
    shader->setUniformValue("cSource", settings.variables[2].source);
    shader->setUniformValue("palette", settings.palette);
    shader->setUniformValue("frequency", float(settings.frequency));
    shader->setUniformValue("phase", float(settings.phase));
    shader->setUniformValue("smoothing", float(settings.smoothing));
    shader->setUniformValue("showMarker", int(hasPoint));
    shader->setUniformValue("marker",
                            QVector2D(float((px - cx) / span * height() + width() * .5L) * ratio,
                                      float((py - cy) / span * height() + height() * .5L) * ratio));
    if (!gradientTexture)
        glGenTextures(1, &gradientTexture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_1D, gradientTexture);
    if (uploadedGradient != settings.gradient) {
        QVector<float> pixels;
        pixels.reserve(4096 * 3);
        for (int i = 0; i < 4096; ++i) {
            QColor colour = gradientColour(settings.gradient, double(i) / 4095);
            pixels << colour.redF() << colour.greenF() << colour.blueF();
        }
        glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB16F, 4096, 0, GL_RGB, GL_FLOAT, pixels.constData());
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        uploadedGradient = settings.gradient;
    }
    shader->setUniformValue("gradientTexture", 0);
    GLuint interiorTexture = 0;
    glGenTextures(1, &interiorTexture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_1D, interiorTexture);
    QVector<float> interiorPixels;
    const auto stops = colouring.interiorStops.isEmpty() ? settings.gradient : colouring.interiorStops;
    for (int i = 0; i < 1024; ++i) {
        QColor colour = gradientColour(stops, double(i) / 1023);
        interiorPixels << colour.redF() << colour.greenF() << colour.blueF();
    }
    glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB16F, 1024, 0, GL_RGB, GL_FLOAT, interiorPixels.constData());
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    shader->setUniformValue("interiorTexture", 1);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glDeleteTextures(1, &interiorTexture);
    glActiveTexture(GL_TEXTURE0);
    shader->release();
    if (exportSize.isEmpty() && settings.orbit) {
        QPainter painter(this);
        drawOrbit(painter, size());
    }
}

void View::mousePressEvent(QMouseEvent *e) {
    setFocus();
    if (activated)
        activated();
    last = e->position().toPoint();
    dragging = e->button() == Qt::LeftButton;
    picking = e->button() == Qt::RightButton;
    inspect(e->position(), picking);
}

void View::mouseMoveEvent(QMouseEvent *e) {
    QPoint now = e->position().toPoint();
    if (dragging) {
        cx -= (now.x() - last.x()) * span / height();
        cy += (now.y() - last.y()) * span / height();
        update();
    } else if (picking)
        inspect(e->position(), true);
    last = now;
}

void View::mouseReleaseEvent(QMouseEvent *e) {
    if (e->button() == Qt::LeftButton)
        dragging = false;
    if (e->button() == Qt::RightButton)
        picking = false;
}

void View::wheelEvent(QWheelEvent *e) {
    auto before = coordinate(e->position());
    long double next = span * std::pow(.82L, e->angleDelta().y() / 120.L);
    if (next > 0 && std::isfinite(next)) {
        span = next;
        auto after = coordinate(e->position());
        cx += before.first - after.first;
        cy += before.second - after.second;
        const double x = double(cx), y = double(cy);
        const double spacing = qMax(std::abs(std::nextafter(x, INFINITY) - x),
                                    std::abs(std::nextafter(y, INFINITY) - y));
        if (status)
            status(QString("Span %1%2")
                       .arg(double(span), 0, 'g', 8)
                       .arg(span / (height() * devicePixelRatioF()) <= spacing
                                ? " — double-precision limit: pixels can no longer resolve "
                                  "distinct coordinates"
                                : ""));
        update();
    }
}

void View::keyPressEvent(QKeyEvent *e) {
    if (e->key() == Qt::Key_Home)
        reset();
    else
        QOpenGLWidget::keyPressEvent(e);
}

View::View(QWidget *parent) : QOpenGLWidget(parent) {
    setMinimumSize(1, 1);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
}

View::~View() {
    if (context()) {
        makeCurrent();
        delete shader;
        if (ready) {
            glDeleteVertexArrays(1, &vao);
            glDeleteTextures(1, &gradientTexture);
        }
        doneCurrent();
    }
}

std::pair<long double, long double> View::coordinate(QPointF pos) const {
    return {cx + (pos.x() - width() * .5L) * span / height(),
            cy + (height() * .5L - pos.y()) * span / height()};
}

void View::reset() {
    cx = settings.variables[2].source == Pixel ? -.5L : 0;
    cy = 0;
    span = 3;
    update();
}

bool View::apply() {
    try {
        QString next = Equation::compile(settings.equation, settings.names);
        const auto &z = settings.variables[3];
        QString start = z.source == Pixel        ? "pixel"
                        : z.source == Expression ? Equation::compile(z.equation, settings.names)
                                                 : "input3";
        if (z.source == Expression &&
            QRegularExpression("\\b(z|n)\\b|historyAt").match(start).hasMatch())
            throw std::runtime_error("Initial Z cannot refer to z or n before iteration starts.");
        if (z.source != Pixel && z.source != Expression && z.real == 0 && z.imag == 0)
            start = "dvec2(0.0,0.0)";
        QString old = expr, oldInitial = initial;
        expr = next;
        initial = start;
        if (ready) {
            makeCurrent();
            bool ok = rebuild();
            doneCurrent();
            if (!ok) {
                expr = old;
                initial = oldInitial;
                return false;
            }
        }
        update();
        return true;
    } catch (const std::exception &e) {
        lastError = e.what();
        return false;
    }
}

QImage View::renderImage(QSize size) {
    makeCurrent();
    GLint limit;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &limit);
    if (size.width() > limit || size.height() > limit) {
        doneCurrent();
        return {};
    }
    QImage image;
    {
        QOpenGLFramebufferObject framebuffer(size);
        if (framebuffer.isValid()) {
            framebuffer.bind();
            exportSize = size;
            paintGL();
            image = framebuffer.toImage();
            exportSize = {};
            glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());
        }
    }
    doneCurrent();
    if (!image.isNull() && settings.orbit) {
        QPainter painter(&image);
        drawOrbit(painter, size);
    }
    return image;
}

bool View::check() {
    makeCurrent();
    bool ok = shader && glGetError() == GL_NO_ERROR;
    doneCurrent();
    return ok;
}
