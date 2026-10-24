#include <QtWidgets>
#include <QOpenGLWidget>
#include <QOpenGLFunctions_4_0_Core>
#include <QOpenGLShaderProgram>
#include <cmath>
#include <functional>
#include <stdexcept>
#include <complex>
#include <QOpenGLFramebufferObject>
#include "workspace.h"
#include "gradient_editor.h"

// Expressions compile to GLSL rather than being evaluated on the CPU per pixel.
class Equation {
    QString text; int pos=0;
    void space(){while(pos<text.size()&&text[pos].isSpace())++pos;}
    bool eat(QChar c){space();if(pos<text.size()&&text[pos]==c){++pos;return true;}return false;}
    QString sum(){QString a=product();for(;;){if(eat('+'))a="("+a+"+"+product()+")";else if(eat('-'))a="("+a+"-"+product()+")";else return a;}}
    QString product(){QString a=unary();for(;;){if(eat('*'))a="mul("+a+","+unary()+")";else if(eat('/'))a="divide("+a+","+unary()+")";else return a;}}
    QString unary(){if(eat('-'))return "(-"+unary()+")";if(eat('+'))return unary();return power();}
    QString power(){QString a=atom();if(eat('^'))a="cpow("+a+","+unary()+")";return a;}
    QString atom(){
        space();if(eat('(')){QString a=sum();if(!eat(')'))throw std::runtime_error("Expected closing parenthesis");return a;}
        int start=pos;
        if(pos<text.size()&&(text[pos].isDigit()||text[pos]=='.')){
            bool digit=false;while(pos<text.size()&&text[pos].isDigit()){++pos;digit=true;}if(pos<text.size()&&text[pos]=='.'){++pos;while(pos<text.size()&&text[pos].isDigit()){++pos;digit=true;}}
            if(!digit)throw std::runtime_error("Invalid number");
            if(pos<text.size()&&(text[pos]=='e'||text[pos]=='E')){++pos;if(pos<text.size()&&(text[pos]=='+'||text[pos]=='-'))++pos;int s=pos;while(pos<text.size()&&text[pos].isDigit())++pos;if(s==pos)throw std::runtime_error("Invalid exponent");}
            bool ok;double n=text.mid(start,pos-start).toDouble(&ok);if(!ok||!std::isfinite(n))throw std::runtime_error("Invalid number");return "dvec2("+QString::number(n,'g',17)+(std::floor(n)==n&&std::abs(n)<1e15?".0":"")+",0.0)";
        }
        while(pos<text.size()&&(text[pos].isLetterOrNumber()||text[pos]=='_'))++pos;
        QString id=text.mid(start,pos-start);
        if(id=="z_n")id="z";
        if(id=="z"||id=="c"||id=="pixel"||id=="a"||id=="b")return id;
        if(id=="p"||id=="n")return "dvec2("+id+",0.0)";
        if(id=="i")return "dvec2(0.0,1.0)";
        if(id=="pi")return "dvec2(3.141592653589793,0.0)";
        QString fn;
        if(id=="sin")fn="csin";
        if(id=="cos")fn="ccos";
        if(id=="exp")fn="cexp";
        if(id=="log")fn="clog";
        if(id=="conj")fn="conjugate";
        if(id=="abs")fn="componentAbs";
        if(id=="norm")fn="magnitude";
        if(fn.isEmpty())throw std::runtime_error(("Unknown name: "+id).toStdString());
        if(!eat('('))throw std::runtime_error("Expected function argument");
        QString a=sum();
        if(!eat(')'))throw std::runtime_error("Expected closing parenthesis");
        return fn+"("+a+")";
    }
public:
    static QString compile(QString source){if(source.size()>512)throw std::runtime_error("Equation is too long (maximum 512 characters)");Equation e;e.text=source;QString result=e.sum();e.space();if(e.pos!=source.size())throw std::runtime_error("Unexpected character in equation");return result;}
};

enum Source { Constant, Mouse, Pixel, Expression };
struct Variable {
    int source=Constant;
    double real=0,imag=0;
    QString equation="0";
    int modifiers=0;
};
struct Settings {
    QString equation="z^p+c";
    int iterations=400, palette=0;
    double p=2,bailout=16,frequency=.025,phase=0,smoothing=1;
    QJsonArray gradient={QJsonObject{{"position",0.0},{"colour","#ffffff"}},QJsonObject{{"position",.25},{"colour","#5599ff"}},QJsonObject{{"position",.65},{"colour","#e060bb"}},QJsonObject{{"position",1.0},{"colour","#000000"}}};
    QString gradientName="Default";
    bool orbit=false;int orbitSteps=100;
    Variable variables[4]={{Constant,1,0,"0",int(Qt::ShiftModifier)},
        {Constant,0,0,"0",int(Qt::ControlModifier)},
        {Pixel,-.745,.186,"0",0},{Constant,0,0,"0",int(Qt::AltModifier)}};
};

class View:public QOpenGLWidget,protected QOpenGLFunctions_4_0_Core {
public:
    Settings settings;
    long double cx=-.5L,cy=0,span=3;
    std::function<void(double,double,Qt::KeyboardModifiers)> mousePicked;
    std::function<void(QString)> status;
    std::function<void()> activated;
    QString lastError;
private:
    QOpenGLShaderProgram *shader=nullptr;
    GLuint gradientTexture=0;
    QJsonArray uploadedGradient;
    GLuint vao=0;QPoint last;bool dragging=false,picking=false,hasPoint=false;
    double px=0,py=0;
    QString expr="cpow(z,dvec2(p,0.0))+c",initial="dvec2(0.0,0.0)";
    bool ready=false;
    QSize exportSize;
    void drawOrbit(QPainter &painter,QSize size){
        if(!settings.orbit||!hasPoint)return;
        using C=std::complex<double>;C pixel(px,py),z(0);int iteration=0;
        auto value=[&](QString name)->C{if(name=="z"||name=="z_n")return z;if(name=="pixel")return pixel;if(name=="p")return C(settings.p);if(name=="n")return C(iteration);int index=name=="a"?0:name=="b"?1:name=="c"?2:-1;if(index<0)throw std::runtime_error("Unknown variable");const auto &v=settings.variables[index];return v.source==Pixel?pixel:C(v.real,v.imag);};
        const auto &start=settings.variables[3];
        try{z=start.source==Pixel?pixel:start.source==Expression?ComplexExpression::evaluate(start.equation,value):C(start.real,start.imag);
            auto screen=[&](C point){return QPointF(double((point.real()-cx)/span*size.height()+size.width()*.5L),double((cy-point.imag())/span*size.height()+size.height()*.5L));};
            painter.setRenderHint(QPainter::Antialiasing);painter.setPen(QPen(QColor("#ffeb55"),1.5));
            for(iteration=0;iteration<settings.orbitSteps;++iteration){C next=ComplexExpression::evaluate(settings.equation,value);if(!std::isfinite(next.real())||!std::isfinite(next.imag()))break;QPointF a=screen(z),b=screen(next);if(std::abs(a.x())<1e7&&std::abs(a.y())<1e7&&std::abs(b.x())<1e7&&std::abs(b.y())<1e7){painter.drawLine(a,b);painter.drawEllipse(b,2,2);}z=next;if(std::abs(z)>settings.bailout)break;}
        }catch(const std::exception &e){if(status)status(e.what());}
    }
    QString fragment()const{return QString(R"GLSL(#version 400 core
out vec4 frag;
uniform dvec2 center,constantC,paramA,paramB,z0;
uniform double scale,p,bailout;
uniform vec2 resolution,marker;
uniform int iterations,aSource,bSource,cSource,palette,showMarker,quadratic;
uniform float frequency,phase,smoothing;
uniform sampler1D gradientTexture;
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
void main(){
 dvec2 pixel=center+dvec2(gl_FragCoord.xy-resolution*.5)*(scale/double(resolution.y));
 dvec2 c=cSource==2?pixel:constantC,a=aSource==2?pixel:paramA,b=bSource==2?pixel:paramB;
 double n=0.0;dvec2 z=%1;
 int count=0;double radius=0.0;
 bool inside=false;
 if(quadratic!=0){double y2=c.y*c.y,x=c.x-.25,q=x*x+y2;inside=q*(q+x)<=.25*y2||(c.x+1.0)*(c.x+1.0)+y2<=.0625;}
 if(inside)count=iterations;
 else for(int j=0;j<10000;j++){if(j>=iterations)break;n=double(j);z=%2;radius=dot(z,z);count=j+1;if(isnan(radius)||isinf(radius)||radius>bailout*bailout)break;}
 vec3 rgb=vec3(.016,.020,.034);
 if(count<iterations){float t=float(count);if(radius>1.0&&!isinf(radius)&&p>1.0)t+=smoothing*(1.0-log(max(log(float(radius))*.5,.0001))/log(float(p)));
 float v=t*frequency+phase;
 if(palette==0)rgb=.5+.5*cos(6.2831853*(v+vec3(.02,.19,.36)));
 else if(palette==1)rgb=.5+.5*cos(6.2831853*(v+vec3(.0,.08,.20)));
 else if(palette==2)rgb=.5+.5*cos(6.2831853*(v+vec3(.50,.32,.10)));
 else rgb=vec3(.5+.5*cos(6.2831853*v));
 rgb*=.8+.2*(1.0-exp(-max(t,0.0)*.06));
 if(palette>=4)rgb=gradient(1.0-abs(2.0*fract(v*.5)-1.0));
 }else if(palette>=4)rgb=vec3(0);
 if(showMarker!=0){vec2 d=abs(gl_FragCoord.xy-marker);if((d.x<1&&d.y<8)||(d.y<1&&d.x<8))rgb=vec3(1);else if((d.x<2&&d.y<9)||(d.y<2&&d.x<9))rgb=vec3(0);}
 frag=vec4(rgb,1);
})GLSL").arg(initial,expr);}
    bool rebuild(){
        auto *candidate=new QOpenGLShaderProgram;
        const char *vertex="#version 400 core\nvoid main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2.-1.,0,1);}";
        if(!candidate->addShaderFromSourceCode(QOpenGLShader::Vertex,vertex)||!candidate->addShaderFromSourceCode(QOpenGLShader::Fragment,fragment())||!candidate->link()){
            lastError=candidate->log();delete candidate;return false;
        }
        delete shader;shader=candidate;lastError.clear();return true;
    }
    void uniform(const char *name,double x){glUniform1d(shader->uniformLocation(name),x);}
    void uniform2(const char *name,double x,double y){glUniform2d(shader->uniformLocation(name),x,y);}
    void inspect(QPointF pos,bool link){
        auto [x,y]=coordinate(pos);px=double(x);py=double(y);hasPoint=true;
        if(status)status(QString("%1 %2 %3i    |    span %4").arg(px,0,'g',16).arg(py<0?"−":"+").arg(std::abs(py),0,'g',16).arg(double(span),0,'g',5));
        if(link&&mousePicked)mousePicked(px,py,QGuiApplication::keyboardModifiers());
        update();
    }
protected:
    void initializeGL()override{
        if(!initializeOpenGLFunctions()){lastError="OpenGL 4.0 is required";return;}
        ready=true;glGenVertexArrays(1,&vao);glBindVertexArray(vao);rebuild();
    }
    void paintGL()override{
        glClearColor(.016f,.020f,.034f,1);glClear(GL_COLOR_BUFFER_BIT);if(!shader)return;
        shader->bind();glBindVertexArray(vao);
        float ratio=float(devicePixelRatioF());float w=exportSize.isEmpty()?width()*ratio:exportSize.width(),h=exportSize.isEmpty()?height()*ratio:exportSize.height();glViewport(0,0,int(w),int(h));
        const bool quadratic=expr=="(cpow(z,dvec2(p,0.0))+c)"&&initial=="dvec2(0.0,0.0)"&&settings.p==2&&settings.variables[2].source==Pixel&&settings.bailout>=2;
        shader->setUniformValue("quadratic",int(quadratic));
        uniform2("center",double(cx),double(cy));uniform("scale",double(span));uniform("p",settings.p);uniform("bailout",settings.bailout);
        const char *names[]={"paramA","paramB","constantC","z0"};
        for(int i=0;i<4;++i)uniform2(names[i],settings.variables[i].real,settings.variables[i].imag);
        shader->setUniformValue("aSource",settings.variables[0].source);shader->setUniformValue("bSource",settings.variables[1].source);
        shader->setUniformValue("resolution",QVector2D(w,h));shader->setUniformValue("iterations",settings.iterations);shader->setUniformValue("cSource",settings.variables[2].source);shader->setUniformValue("palette",settings.palette);shader->setUniformValue("frequency",float(settings.frequency));shader->setUniformValue("phase",float(settings.phase));shader->setUniformValue("smoothing",float(settings.smoothing));
        shader->setUniformValue("showMarker",int(hasPoint));shader->setUniformValue("marker",QVector2D(float((px-cx)/span*height()+width()*.5L)*ratio,float((py-cy)/span*height()+height()*.5L)*ratio));
        if(!gradientTexture)glGenTextures(1,&gradientTexture);
        glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_1D,gradientTexture);
        if(uploadedGradient!=settings.gradient){QVector<float> pixels;pixels.reserve(4096*3);for(int i=0;i<4096;++i){QColor colour=gradientColour(settings.gradient,double(i)/4095);pixels<<colour.redF()<<colour.greenF()<<colour.blueF();}glTexImage1D(GL_TEXTURE_1D,0,GL_RGB16F,4096,0,GL_RGB,GL_FLOAT,pixels.constData());glTexParameteri(GL_TEXTURE_1D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_1D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_1D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);uploadedGradient=settings.gradient;}shader->setUniformValue("gradientTexture",0);
        glDrawArrays(GL_TRIANGLES,0,3);shader->release();
        if(exportSize.isEmpty()&&settings.orbit){QPainter painter(this);drawOrbit(painter,size());}
    }
    void mousePressEvent(QMouseEvent *e)override{setFocus();if(activated)activated();last=e->position().toPoint();dragging=e->button()==Qt::LeftButton;picking=e->button()==Qt::RightButton;inspect(e->position(),picking);}
    void mouseMoveEvent(QMouseEvent *e)override{
        QPoint now=e->position().toPoint();if(dragging){cx-=(now.x()-last.x())*span/height();cy+=(now.y()-last.y())*span/height();update();}else if(picking)inspect(e->position(),true);last=now;
    }
    void mouseReleaseEvent(QMouseEvent *e)override{if(e->button()==Qt::LeftButton)dragging=false;if(e->button()==Qt::RightButton)picking=false;}
    void wheelEvent(QWheelEvent *e)override{
        auto before=coordinate(e->position());long double next=span*std::pow(.82L,e->angleDelta().y()/120.L);
        if(next>0&&std::isfinite(next)){span=next;auto after=coordinate(e->position());cx+=before.first-after.first;cy+=before.second-after.second;
            const double x=double(cx),y=double(cy);
            const double spacing=qMax(std::abs(std::nextafter(x,INFINITY)-x),std::abs(std::nextafter(y,INFINITY)-y));
            if(status)status(QString("Span %1%2").arg(double(span),0,'g',8).arg(span/(height()*devicePixelRatioF())<=spacing?" — double-precision limit: pixels can no longer resolve distinct coordinates":""));
            update();}
    }
    void keyPressEvent(QKeyEvent *e)override{
        if(e->key()==Qt::Key_Home)reset();else QOpenGLWidget::keyPressEvent(e);
    }
public:
    explicit View(QWidget *parent=nullptr):QOpenGLWidget(parent){setMinimumSize(1,1);setFocusPolicy(Qt::StrongFocus);setMouseTracking(true);}
    ~View(){if(context()){makeCurrent();delete shader;if(ready){glDeleteVertexArrays(1,&vao);glDeleteTextures(1,&gradientTexture);}doneCurrent();}}
    std::pair<long double,long double> coordinate(QPointF pos)const{return {cx+(pos.x()-width()*.5L)*span/height(),cy+(height()*.5L-pos.y())*span/height()};}
    void reset(){cx=settings.variables[2].source==Pixel?-.5L:0;cy=0;span=3;update();}
    bool apply(){
        try{QString next=Equation::compile(settings.equation);
            const auto &z=settings.variables[3];
            QString start=z.source==Pixel?"pixel":z.source==Expression?Equation::compile(z.equation):"z0";
            if(z.source==Expression&&QRegularExpression("\\b(z|n)\\b").match(start).hasMatch())throw std::runtime_error("Initial Z cannot refer to z or n before iteration starts.");
            if(z.source!=Pixel&&z.source!=Expression&&z.real==0&&z.imag==0)start="dvec2(0.0,0.0)";
            QString old=expr,oldInitial=initial;expr=next;initial=start;
            if(ready&&(expr!=old||initial!=oldInitial)){makeCurrent();bool ok=rebuild();doneCurrent();if(!ok){expr=old;initial=oldInitial;return false;}}update();return true;
        }catch(const std::exception &e){lastError=e.what();return false;}
    }
    QImage renderImage(QSize size){
        makeCurrent();GLint limit;glGetIntegerv(GL_MAX_TEXTURE_SIZE,&limit);if(size.width()>limit||size.height()>limit){doneCurrent();return {};}
        QImage image;{QOpenGLFramebufferObject framebuffer(size);if(framebuffer.isValid()){framebuffer.bind();exportSize=size;paintGL();image=framebuffer.toImage();exportSize={};glBindFramebuffer(GL_FRAMEBUFFER,defaultFramebufferObject());}}doneCurrent();
        if(!image.isNull()&&settings.orbit){QPainter painter(&image);drawOrbit(painter,size);}return image;
    }
    bool check(){makeCurrent();bool ok=shader&&glGetError()==GL_NO_ERROR;doneCurrent();return ok;}
};

class NumericSlider:public QWidget {
    QAbstractSpinBox *editor;
    QDoubleSpinBox *realEditor;
    QSpinBox *intEditor;
    QSlider *slider;
    double lower,upper;
    double value()const{return realEditor?realEditor->value():intEditor->value();}
    void sync(){QSignalBlocker block(slider);slider->setValue(qRound(qBound(0.0,(value()-lower)/(upper-lower),1.0)*10000));}
public:
    explicit NumericSlider(QAbstractSpinBox *input):editor(input),realEditor(qobject_cast<QDoubleSpinBox*>(input)),intEditor(qobject_cast<QSpinBox*>(input)){
        double minimum=realEditor?realEditor->minimum():intEditor->minimum();
        double maximum=realEditor?realEditor->maximum():intEditor->maximum();
        lower=qMax(minimum,value()-1);upper=qMin(maximum,value()+1);
        if(intEditor){lower=minimum;upper=maximum;}
        auto *layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);layout->setSpacing(4);
        auto *row=new QHBoxLayout;row->addWidget(editor,1);auto *button=new QToolButton;button->setText("↔");button->setToolTip("Configure slider range or return to numeric input");row->addWidget(button);layout->addLayout(row);
        slider=new QSlider(Qt::Horizontal);slider->setRange(0,10000);slider->hide();layout->addWidget(slider);
        connect(slider,&QSlider::valueChanged,this,[this](int position){double next=lower+(upper-lower)*position/10000.;if(realEditor)realEditor->setValue(next);else intEditor->setValue(qRound(next));});
        if(realEditor)connect(realEditor,&QDoubleSpinBox::valueChanged,this,[this]{sync();});
        else connect(intEditor,&QSpinBox::valueChanged,this,[this]{sync();});
        connect(button,&QToolButton::clicked,this,[this,minimum,maximum]{
            QDialog dialog(this);dialog.setWindowTitle("Slider range");QFormLayout form(&dialog);
            QCheckBox enabled("Show slider");enabled.setChecked(!slider->isVisibleTo(this)?false:true);
            QDoubleSpinBox from,to;for(auto *field:{&from,&to}){field->setDecimals(realEditor?realEditor->decimals():0);field->setRange(minimum,maximum);}
            from.setValue(lower);to.setValue(upper);form.addRow(&enabled);form.addRow("Minimum",&from);form.addRow("Maximum",&to);
            QLabel message;form.addRow(&message);QDialogButtonBox buttons(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);form.addRow(&buttons);
            connect(&buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
            connect(&buttons,&QDialogButtonBox::accepted,&dialog,[&]{if(from.value()>=to.value()){message.setText("Minimum must be less than maximum.");return;}dialog.accept();});
            if(dialog.exec()==QDialog::Accepted){lower=from.value();upper=to.value();slider->setVisible(enabled.isChecked());slider->setToolTip(QString("%1 to %2").arg(lower,0,'g',12).arg(upper,0,'g',12));sync();}
        });sync();
    }
    QJsonObject state()const{return {{"lower",lower},{"upper",upper},{"enabled",!slider->isHidden()}};}
    void restoreState(QJsonObject state){double lo=state["lower"].toDouble(NAN),hi=state["upper"].toDouble(NAN);double minimum=realEditor?realEditor->minimum():intEditor->minimum(),maximum=realEditor?realEditor->maximum():intEditor->maximum();if(!std::isfinite(lo)||!std::isfinite(hi)||lo>=hi||lo<minimum||hi>maximum)return;lower=lo;upper=hi;slider->setVisible(state["enabled"].toBool());sync();}
    bool test(){lower=realEditor?-0.5:16;upper=realEditor?0.5:116;slider->setValue(5000);bool ok=std::abs(value()-(lower+upper)*.5)<1e-8;slider->setValue(10000);return ok&&std::abs(value()-upper)<1e-8;}
};

class Playground:public QMainWindow {
#include "playground_tools.inc"
    View *left,*right;QDockWidget *dock;QComboBox *target,*preset,*palette;QLineEdit *equation;QLabel *error,*coordinates;
    QDoubleSpinBox *exponent,*bailout,*frequency,*phase,*smoothing;QSpinBox *iterations;
    QList<View*> views;
    int nextNumber=1;
    void viewOperations(){
        QDialog dialog(this);dialog.setWindowTitle("Copy settings / Julia counterpart");dialog.setMinimumWidth(390);QFormLayout form(&dialog);
        QComboBox source,destination,operation;
        for(auto *view:views){source.addItem(viewName(view));destination.addItem(viewName(view));}
        destination.setCurrentIndex(views.indexOf(active()));source.setCurrentIndex(active()==views.first()&&views.size()>1?1:0);
        operation.addItems({"Copy all settings","Create Julia counterpart"});
        form.addRow("Source view",&source);form.addRow("Target view",&destination);form.addRow("Operation",&operation);
        QLabel note("Copy replaces all fractal settings. Julia conversion preserves the target's colours and leaves mouse picking local to each view.");note.setWordWrap(true);form.addRow(&note);
        QLabel message;message.setWordWrap(true);form.addRow(&message);
        QDialogButtonBox buttons(QDialogButtonBox::Apply|QDialogButtonBox::Close);form.addRow(&buttons);connect(&buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
        connect(buttons.button(QDialogButtonBox::Apply),&QPushButton::clicked,&dialog,[&]{
            View *from=views[source.currentIndex()],*to=views[destination.currentIndex()];
            if(from==to){message.setText("Choose different source and target views.");return;}
            bool ok=operation.currentIndex()==0?copySettings(from,to):makeJulia(from,to);
            message.setText(ok?"Applied successfully.":error->text());
        });dialog.exec();
    }
    QString viewName(View *view)const{return QString("View %1").arg(view->property("number").toInt());}
    void refreshViews(View *selected){
        {QSignalBlocker block(target);target->clear();for(auto *view:views)target->addItem(viewName(view));target->setCurrentIndex(qMax(0,int(views.indexOf(selected))));}load();
    }
    void removeView(){
        if(views.size()==1)return;
        leaveFullscreen();View *view=active();auto *parent=qobject_cast<QSplitter*>(view->parentWidget());
        int removedNumber=view->property("number").toInt();
        routes.erase(std::remove_if(routes.begin(),routes.end(),[&](PickRoute r){return r.source==removedNumber||r.target==removedNumber;}),routes.end());
        QJsonArray remainingTracks;for(auto track:tracks)if(track.toObject()["view"].toInt()!=removedNumber)remainingTracks.append(track);tracks=remainingTracks;
        views.removeOne(view);
        view->setParent(nullptr);delete view;
        if(parent!=root&&parent->count()==1){QWidget *remaining=parent->widget(0);auto *grand=qobject_cast<QSplitter*>(parent->parentWidget());int index=grand->indexOf(parent);remaining->setParent(nullptr);grand->replaceWidget(index,remaining);delete parent;remaining->show();}
        QHash<int,int> renumber;for(int i=0;i<views.size();++i){renumber[views[i]->property("number").toInt()]=i+1;views[i]->setProperty("number",i+1);}
        for(auto &route:routes){route.source=renumber.value(route.source);route.target=renumber.value(route.target);}
        for(int i=0;i<tracks.size();++i){auto track=tracks[i].toObject();track["view"]=renumber.value(track["view"].toInt());tracks[i]=track;}nextNumber=views.size()+1;
        refreshViews(views.first());
    }
    bool copySettings(View *source,View *destination){
        Settings previous=destination->settings;destination->settings=source->settings;
        if(!destination->apply()){destination->settings=previous;error->setText(destination->lastError);return false;}
        if(active()==destination)load();
        return true;
    }
    QSplitter *root;
    QToolBar *toolbar;
    bool fullscreenMode=false,wasMaximized=false,dockWasVisible=false;
    QList<QWidget*> fullscreenHidden;
    void bindView(View *view){
        view->mousePicked=[this,view](double x,double y,Qt::KeyboardModifiers m){picked(view,x,y,m);};
        view->status=[this](QString text){coordinates->setText(text);};
        view->activated=[this,view]{target->setCurrentIndex(int(views.indexOf(view)));};
    }
    void balance(){for(auto *s:centralWidget()->findChildren<QSplitter*>()){QList<int> sizes;for(int i=0;i<s->count();++i)sizes<<1000;s->setSizes(sizes);}}
    void splitView(Qt::Orientation orientation){
        if(fullscreenMode)leaveFullscreen();
        View *old=active();auto *parent=qobject_cast<QSplitter*>(old->parentWidget());int index=parent->indexOf(old);
        auto *branch=new QSplitter(orientation);branch->setHandleWidth(1);branch->setChildrenCollapsible(false);
        parent->replaceWidget(index,branch);branch->addWidget(old);
        auto *next=new View;next->settings=old->settings;next->cx=old->cx;next->cy=old->cy;next->span=old->span;next->apply();branch->addWidget(next);
        next->setProperty("number",nextNumber++);views<<next;bindView(next);refreshViews(next);branch->setSizes({1000,1000});
    }
    void leaveFullscreen(){
        if(!fullscreenMode)return;
        for(auto *widget:fullscreenHidden)widget->show();
        fullscreenHidden.clear();
        toolbar->show();statusBar()->show();dock->setVisible(dockWasVisible);fullscreenMode=false;
        if(wasMaximized)showMaximized();else showNormal();
    }
    void enterFullscreen(bool single){
        if(fullscreenMode)leaveFullscreen();
        wasMaximized=isMaximized();dockWasVisible=dock->isVisible();
        if(single){QWidget *node=active();while(node!=centralWidget()){QWidget *parent=node->parentWidget();if(auto *s=qobject_cast<QSplitter*>(parent)){for(int i=0;i<s->count();++i){QWidget *other=s->widget(i);if(other!=node&&!other->isHidden()){fullscreenHidden<<other;other->hide();}}}node=parent;}}
        toolbar->hide();dock->hide();statusBar()->hide();fullscreenMode=true;showFullScreen();active()->setFocus();
    }
    struct VariableControls {QComboBox *source,*modifier;QWidget *values,*picking;QDoubleSpinBox *real,*imag;QLineEdit *expression;};
    VariableControls controls[4];
    void visibility(int i){auto &u=controls[i];int mode=u.source->currentIndex();u.values->setVisible(mode==Constant||mode==Mouse);u.picking->setVisible(mode==Mouse);u.expression->setVisible(mode==Expression);}
    void addVariable(QFormLayout *form,int i){
        auto &u=controls[i];auto *box=new QGroupBox(QStringList{"a","b","c","Initial z"}[i]);auto *rows=new QFormLayout(box);
        u.source=new QComboBox;u.source->addItems({"Constant","Mouse picked","Pixel coordinate"});if(i==3)u.source->addItem("Equation");rows->addRow("Source",u.source);
        u.values=new QWidget;auto *values=new QFormLayout(u.values);values->setContentsMargins(0,0,0,0);u.real=number(-1e6,1e6,12);u.imag=number(-1e6,1e6,12);values->addRow("Real",new NumericSlider(u.real));values->addRow("Imaginary",new NumericSlider(u.imag));rows->addRow(u.values);
        u.picking=new QWidget;auto *picks=new QFormLayout(u.picking);picks->setContentsMargins(0,0,0,0);u.modifier=new QComboBox;
        for(int mask=0;mask<8;++mask){QStringList names;if(mask&1)names<<"Shift";if(mask&2)names<<"Ctrl";if(mask&4)names<<"Alt";int flags=((mask&1)?int(Qt::ShiftModifier):0)|((mask&2)?int(Qt::ControlModifier):0)|((mask&4)?int(Qt::AltModifier):0);u.modifier->addItem(names.isEmpty()?"Right button":names.join(" + ")+" + Right button",flags);}
        picks->addRow("Pick with",u.modifier);rows->addRow(u.picking);u.expression=new QLineEdit;u.expression->setMaxLength(512);u.expression->setPlaceholderText("e.g. a+b*i");rows->addRow(u.expression);form->addRow(box);
        connect(u.source,&QComboBox::currentIndexChanged,this,[this,i]{visibility(i);apply();});connect(u.modifier,&QComboBox::currentIndexChanged,this,[this]{apply();});for(auto *value:{u.real,u.imag})connect(value,&QDoubleSpinBox::valueChanged,this,[this]{apply();});connect(u.expression,&QLineEdit::returnPressed,this,[this]{apply();});
    }
    bool loading=false;
    View *active()const{return views.value(qMax(0,target->currentIndex()),views.first());}
    QDoubleSpinBox *number(double min,double max,int decimals=8){auto *v=new QDoubleSpinBox;v->setRange(min,max);v->setDecimals(decimals);v->setSingleStep(.005);v->setKeyboardTracking(false);return v;}
    void load(){loading=true;const auto &s=active()->settings;equation->setText(s.equation);exponent->setValue(s.p);bailout->setValue(s.bailout);iterations->setValue(s.iterations);palette->setCurrentIndex(s.palette==5?qMax(5,palette->findText(s.gradientName)):s.palette);frequency->setValue(s.frequency);phase->setValue(s.phase);smoothing->setValue(s.smoothing);
        for(int i=0;i<4;++i){const auto &v=s.variables[i];auto &u=controls[i];u.source->setCurrentIndex(v.source);u.real->setValue(v.real);u.imag->setValue(v.imag);u.expression->setText(v.equation);u.modifier->setCurrentIndex(u.modifier->findData(v.modifiers));visibility(i);}loading=false;
    }
    void apply(){if(loading)return;auto &s=active()->settings;s.equation=equation->text();s.p=exponent->value();s.bailout=bailout->value();s.iterations=iterations->value();s.palette=qMin(5,palette->currentIndex());if(palette->currentIndex()>=5&&palette->currentIndex()-5<gradientLibrary.size()){auto entry=gradientLibrary[palette->currentIndex()-5].toObject();s.gradientName=entry["name"].toString();s.gradient=entry["stops"].toArray();}s.frequency=frequency->value();s.phase=phase->value();s.smoothing=smoothing->value();
        for(int i=0;i<4;++i){auto &v=s.variables[i];auto &u=controls[i];v.source=u.source->currentIndex();v.real=u.real->value();v.imag=u.imag->value();v.equation=u.expression->text();v.modifiers=u.modifier->currentData().toInt();}
        error->setText(active()->apply()?"":active()->lastError);
    }
    void choosePreset(int index){if(loading)return;auto &s=active()->settings;s=Settings();switch(index){case 1:s.variables[3].source=Pixel;s.variables[2].source=Mouse;break;case 2:s.equation="abs(z)^p+c";break;case 3:s.equation="conj(z)^p+c";break;case 4:s.p=3;break;case 5:s.equation="sin(z)*a+c";s.variables[3].source=Pixel;s.variables[2].source=Mouse;break;}load();apply();active()->reset();}
    bool makeJulia(View *source,View *destination){
        if(source==destination){error->setText("Choose different source and target views.");return false;}
        if(active()==source){apply();if(!error->text().isEmpty())return false;}
        try{
            if(source->settings.variables[2].source!=Pixel)throw std::runtime_error("The source view must use pixel coordinates as c.");
            if(source->settings.variables[3].source==Pixel||source->settings.variables[3].source==Expression)throw std::runtime_error("The source view must have a constant or mouse-picked initial z.");
            if(QRegularExpression("\\bpixel\\b").match(Equation::compile(source->settings.equation)).hasMatch())throw std::runtime_error("Use c rather than pixel in the source recurrence so it can become a constant Julia parameter.");
            Settings previous=destination->settings;
            destination->settings=source->settings;
            auto &s=destination->settings;s.palette=previous.palette;s.frequency=previous.frequency;s.phase=previous.phase;s.smoothing=previous.smoothing;s.gradient=previous.gradient;s.gradientName=previous.gradientName;
            s.variables[3].source=Pixel;s.variables[2]=previous.variables[2];s.variables[2].source=Mouse;
            if(!destination->apply()){destination->settings=previous;throw std::runtime_error(destination->lastError.toStdString());}
            destination->reset();target->setCurrentIndex(views.indexOf(destination));load();error->clear();return true;
        }catch(const std::exception &e){error->setText(e.what());return false;}
    }
    void picked(View *source,double x,double y,Qt::KeyboardModifiers modifiers){
        int flags=int(modifiers&(Qt::ShiftModifier|Qt::ControlModifier|Qt::AltModifier));bool routed=false;
        for(auto route:routes)if(route.source==source->property("number").toInt()&&route.modifiers==flags){if(auto *dest=byNumber(route.target)){auto &v=dest->settings.variables[route.variable];v.source=Mouse;v.real=x;v.imag=y;dest->apply();if(active()==dest)load();routed=true;}}
        if(routed)return;
        pickInto(source,x,y,modifiers);
    }
    void pickInto(View *dest,double x,double y,Qt::KeyboardModifiers modifiers){
        int flags=int(modifiers&(Qt::ShiftModifier|Qt::ControlModifier|Qt::AltModifier));
        int found=-1;
        for(int i=0;i<4;++i){const auto &v=dest->settings.variables[i];if(v.source==Mouse&&v.modifiers==flags){if(found>=0){coordinates->setText("Picking shortcut conflict: assign different modifiers to each mouse-picked variable.");return;}found=i;}}
        if(found<0)return;
        auto &v=dest->settings.variables[found];v.real=x;v.imag=y;
        if(!dest->apply())error->setText(dest->lastError);
        if(active()==dest)load();
    }
public:
    Playground(){
        setWindowTitle("Fractal Playground");resize(1420,900);
        auto *container=new QWidget;auto *layout=new QVBoxLayout(container);layout->setContentsMargins(0,0,0,0);layout->setSpacing(0);
        container->setStyleSheet("QSplitter::handle { background: black; }");
        root=new QSplitter;root->setHandleWidth(1);root->setChildrenCollapsible(false);left=new View;right=new View;right->settings.variables[2].source=Mouse;right->settings.variables[3].source=Pixel;right->cx=0;right->apply();root->addWidget(left);root->addWidget(right);views={left,right};for(auto *view:views)view->setProperty("number",nextNumber++);layout->addWidget(root);setCentralWidget(container);
        dock=new QDockWidget("Playground settings",this);dock->setAllowedAreas(Qt::LeftDockWidgetArea|Qt::RightDockWidgetArea);dock->setMinimumWidth(320);auto *scroll=new QScrollArea;scroll->setWidgetResizable(true);auto *body=new QWidget;auto *form=new QFormLayout(body);form->setSpacing(10);scroll->setWidget(body);dock->setWidget(scroll);addDockWidget(Qt::RightDockWidgetArea,dock);
        toolbar=addToolBar("View");toolbar->setMovable(false);toolbar->addAction(dock->toggleViewAction());auto *reset=toolbar->addAction("Reset active view");connect(reset,&QAction::triggered,this,[this]{active()->reset();});
        auto *panesButton=new QToolButton;panesButton->setText("Layout / Fullscreen");panesButton->setPopupMode(QToolButton::InstantPopup);auto *menu=new QMenu(panesButton);panesButton->setMenu(menu);toolbar->addWidget(panesButton);
        menu->addAction("Centre all dividers",this,[this]{balance();});
        menu->addAction("Split active left / right",this,[this]{splitView(Qt::Horizontal);});
        menu->addAction("Split active top / bottom",this,[this]{splitView(Qt::Vertical);});
        menu->addAction("Fullscreen active view (F11)",this,[this]{enterFullscreen(true);});
        menu->addAction("Fullscreen entire layout (Ctrl+F11)",this,[this]{enterFullscreen(false);});
        auto *escape=new QShortcut(QKeySequence(Qt::Key_Escape),this);connect(escape,&QShortcut::activated,this,[this]{leaveFullscreen();});
        auto *full=new QShortcut(QKeySequence(Qt::Key_F11),this);connect(full,&QShortcut::activated,this,[this]{if(fullscreenMode)leaveFullscreen();else enterFullscreen(true);});
        auto *all=new QShortcut(QKeySequence(Qt::CTRL|Qt::Key_F11),this);connect(all,&QShortcut::activated,this,[this]{if(fullscreenMode)leaveFullscreen();else enterFullscreen(false);});
        menu->addAction("Remove active view",this,[this]{removeView();});
        auto *shortcut=new QShortcut(QKeySequence(Qt::Key_Tab),this);connect(shortcut,&QShortcut::activated,this,[this]{dock->setVisible(!dock->isVisible());});
        target=new QComboBox;target->addItems({"View 1","View 2"});form->addRow("Edit",target);connect(target,&QComboBox::currentIndexChanged,this,[this]{load();});
        toolbar->addAction("Copy / Julia…",this,[this]{viewOperations();});
        preset=new QComboBox;preset->addItems({"Mandelbrot","Julia","Burning ship","Tricorn","Cubic Multibrot","Sine Julia"});form->addRow("Load preset",preset);connect(preset,&QComboBox::activated,this,[this](int i){choosePreset(i);});
        equation=new QLineEdit;equation->setMaxLength(512);form->addRow("z next =",equation);
        auto *applyButton=new QPushButton("Compile equation");form->addRow(applyButton);connect(applyButton,&QPushButton::clicked,this,[this]{apply();});connect(equation,&QLineEdit::returnPressed,this,[this]{apply();});
        error=new QLabel;error->setWordWrap(true);error->setStyleSheet("color: #ff9191");form->addRow(error);
        for(int i=0;i<4;++i)addVariable(form,i);
        exponent=number(-100,100);bailout=number(.001,1e9);form->addRow("Exponent p",exponent);form->addRow("Escape radius",bailout);
        iterations=new QSpinBox;iterations->setRange(16,10000);iterations->setSingleStep(100);iterations->setKeyboardTracking(false);form->addRow("Iterations",iterations);
        palette=new QComboBox;palette->addItems({"Spectrum","Ember","Ocean","Monochrome","White / Colour / Black","Custom gradient"});form->addRow("Palette",palette);frequency=number(.00001,10);frequency->setSingleStep(.005);phase=number(-100,100);phase->setSingleStep(.05);form->addRow("Colour frequency",frequency);form->addRow("Colour phase",phase);smoothing=number(0,1,3);smoothing->setSingleStep(.1);smoothing->setToolTip("0: integer escape bands; 1: continuous escape-time colouring. Most accurate for polynomial escape formulas with p > 1.");form->addRow("Colour smoothing",smoothing);
        for(int row=0;row<form->rowCount();++row){
            auto *item=form->itemAt(row,QFormLayout::FieldRole);
            if(item){if(auto *numeric=qobject_cast<QAbstractSpinBox*>(item->widget())){form->removeWidget(numeric);form->setWidget(row,QFormLayout::FieldRole,new NumericSlider(numeric));}}
        }
        auto *help=new QLabel("<b>Numeric sliders</b><br>Press ↔ next to any number to enable a slider and set its minimum and maximum. Typed values remain available.<br><br><b>Equation language</b><br>z or z_n: current iterate<br>pixel: point in this view<br>a, b, c: independently selected sources<br>p: exponent · n: iteration (starts at 0)<br>a, b: editable complex parameters · i · pi<br>Operators: + − * / ^ and parentheses<br>Functions: sin, cos, exp, log, conj, abs, norm<br>abs is component-wise; norm is magnitude.<br><br><b>Navigation</b><br>Drag left: pan · wheel: zoom at cursor<br>Right-click/drag: pick variables in Mouse mode<br>Each variable has configurable Shift/Ctrl/Alt modifiers.<br>Mouse-picked variables use only their own view. Cross-view picking requires an explicit Picking links route.<br>Initial z has its own source dropdown; Equation mode accepts a, b, c and pixel.<br>Home: reset focused view · Tab: sidebar<br><br>There is no zoom cap. Numerical precision eventually limits detail. Double precision is used for arithmetic; transcendental functions use float precision.<br><br>Presets are editable equations, not separate renderers. Escape-time colouring is used for all formulas; convergent formulas may need different bailout settings.");help->setWordWrap(true);help->setTextFormat(Qt::RichText);form->addRow(help);
        coordinates=new QLabel("Click a point to inspect its complex coordinate");statusBar()->addWidget(coordinates,1);
        for(auto *v:{exponent,bailout,frequency,phase,smoothing})connect(v,&QDoubleSpinBox::valueChanged,this,[this]{apply();});
        connect(iterations,&QSpinBox::valueChanged,this,[this]{apply();});connect(palette,&QComboBox::currentIndexChanged,this,[this]{apply();});
        bindView(left);bindView(right);load();initGradients();setupTools();QTimer::singleShot(0,this,[this]{balance();});
    }
    bool smoke(){
        auto *realTest=new QDoubleSpinBox;realTest->setRange(-10,10);realTest->setDecimals(8);
        auto *integerTest=new QSpinBox;integerTest->setRange(16,10000);
        NumericSlider realSlider(realTest),integerSlider(integerTest);
        bool ok=realSlider.test()&&integerSlider.test()&&left->check()&&right->check();
        for(const QString &s:{QString("z^p+c"),QString("abs(z)^p+c"),QString("conj(z)^p+c"),QString("sin(z)*a+c"),QString("z-(z^3-1)/(3*z^2)+c"),QString("exp(z)+log(c)+cos(z)+norm(b)+i+pi+n")}){left->settings.equation=s;ok=left->apply()&&ok;}
        target->setCurrentIndex(0);left->settings=Settings();left->settings.equation="conj(z)^p+c+a";load();
        ok=makeJulia(left,right)&&ok;
        ok=ok&&right->settings.equation==left->settings.equation&&right->settings.variables[3].source==Pixel&&right->settings.variables[2].source==Mouse;
        picked(right,-.75,.1,Qt::NoModifier);ok=ok&&right->settings.variables[2].real==-.75;
        for(int i=0;i<4;++i){right->settings.variables[i].source=Mouse;}
        picked(right,.125,-.25,Qt::AltModifier);ok=ok&&right->settings.variables[3].real==.125&&right->settings.variables[3].imag==-.25;
        picked(right,.2,.3,Qt::ShiftModifier);ok=ok&&right->settings.variables[0].real==.2;
        picked(right,.4,.5,Qt::ControlModifier);ok=ok&&right->settings.variables[1].imag==.5;
        right->settings.variables[0].modifiers=int(Qt::ShiftModifier|Qt::AltModifier);
        picked(right,.6,.7,Qt::ShiftModifier|Qt::AltModifier);ok=ok&&right->settings.variables[0].real==.6;
        right->settings.variables[3].source=Expression;right->settings.variables[3].equation="a+b*c";ok=right->apply()&&ok;
        for(int i=0;i<3;++i){right->settings.variables[i].source=Pixel;}ok=right->apply()&&ok;right->update();
        left->settings.variables[3].source=Pixel;ok=!makeJulia(left,right)&&ok;
        splitView(Qt::Horizontal);splitView(Qt::Vertical);balance();
        ok=ok&&views.size()==4&&active()==views.last();
        enterFullscreen(true);ok=ok&&isFullScreen()&&toolbar->isHidden();leaveFullscreen();
        enterFullscreen(false);ok=ok&&isFullScreen();leaveFullscreen();
        for(auto *view:views)ok=ok&&!view->isHidden();
        View *third=views[2],*fourth=views[3];third->settings=Settings();fourth->settings.palette=4;fourth->settings.frequency=.123;fourth->settings.phase=.4;fourth->settings.smoothing=.3;
        ok=makeJulia(third,fourth)&&ok;
        ok=ok&&fourth->settings.palette==4&&fourth->settings.frequency==.123&&fourth->settings.phase==.4&&fourth->settings.smoothing==.3;
        ok=copySettings(third,fourth)&&ok;ok=ok&&fourth->settings.variables[2].source==Pixel;
        removeView();ok=ok&&views.size()==3;removeView();removeView();ok=ok&&views.size()==1;removeView();ok=ok&&views.size()==1;
        active()->settings=Settings();active()->settings.palette=5;active()->apply();
        QImage exported=active()->renderImage({320,240});ok=ok&&exported.size()==QSize(320,240);
        QTemporaryDir temporary;QString imagePath=temporary.filePath("test.png");ok=ok&&exported.save(imagePath,"PNG")&&QImage(imagePath).size()==QSize(320,240);
        animationFps=120;auto saved=snapshot();ok=restore(saved)&&ok;ok=ok&&animationFps==120;ok=ok&&active()->settings.palette==5&&views.size()==1;
        auto bad=saved;bad["version"]=99;ok=!restore(bad)&&ok;
        int id=active()->property("number").toInt();tracks={QJsonObject{{"view",id},{"parameter",12},{"time",0.0},{"value",0.0}},QJsonObject{{"view",id},{"parameter",12},{"time",2.0},{"value",1.0}}};animateAt(1);ok=ok&&std::abs(active()->settings.phase-.5)<1e-9;
        history.clear();historyIndex=-1;checkpoint();active()->settings.phase=.75;checkpoint();travel(-1);ok=ok&&std::abs(active()->settings.phase-.5)<1e-9;travel(1);ok=ok&&std::abs(active()->settings.phase-.75)<1e-9;
        auto orbit=ComplexExpression::evaluate("z^2+c",[](QString name){return name=="z"?std::complex<double>(1,1):std::complex<double>(.5,0);});ok=ok&&std::abs(orbit-std::complex<double>(.5,2))<1e-9;
        active()->settings.orbit=true;
        QMouseEvent orbitClick(QEvent::MouseButtonPress,QPointF(20,20),QPointF(20,20),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(active(),&orbitClick);
        active()->renderImage({96,96});active()->update();
        splitView(Qt::Horizontal);View *routeSource=views.first(),*routeTarget=views.last();routes<<PickRoute{routeSource->property("number").toInt(),routeTarget->property("number").toInt(),1,int(Qt::AltModifier)};picked(routeSource,.42,-.13,Qt::AltModifier);ok=ok&&routeTarget->settings.variables[1].real==.42;
        auto multi=snapshot();ok=restore(multi)&&ok;ok=ok&&views.size()==2&&routes.size()==1&&tracks.size()==2;
        QSaveFile workspace(temporary.filePath("workspace.json"));ok=workspace.open(QIODevice::WriteOnly)&&ok;workspace.write(QJsonDocument(multi).toJson());ok=workspace.commit()&&ok;
        active()->renderImage({64,64});
        for(int i=0;i<views.size();++i)ok=ok&&views[i]->property("number").toInt()==i+1;
        target->setCurrentIndex(0);removeView();ok=ok&&views.first()->property("number").toInt()==1;splitView(Qt::Horizontal);ok=ok&&active()->property("number").toInt()==2;
        routes.clear();views[0]->settings.variables[2].source=Mouse;views[1]->settings.variables[2].source=Mouse;views[1]->settings.variables[2].real=.123;picked(views[0],.9,.8,Qt::NoModifier);ok=ok&&views[0]->settings.variables[2].real==.9&&views[1]->settings.variables[2].real==.123;
        GradientBar bar;bar.resize(400,80);bar.stops=Settings().gradient;QMouseEvent addStop(QEvent::MouseButtonPress,QPointF(200,30),QPointF(200,30),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(&bar,&addStop);ok=ok&&bar.stops.size()==5;
        QMouseEvent moveStop(QEvent::MouseMove,QPointF(260,60),QPointF(260,60),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(&bar,&moveStop);ok=ok&&bar.stops[3].toObject()["position"].toDouble()>.65;
        for(int i=0;i<25;++i)gradientLibrary.append(QJsonObject{{"name",QString("Test %1").arg(i)},{"stops",bar.stops}});
        refreshPalettes();ok=ok&&palette->count()>=30;
        QMouseEvent deleteStop(QEvent::MouseButtonPress,QPointF(260,60),QPointF(260,60),Qt::RightButton,Qt::RightButton,Qt::NoModifier);QApplication::sendEvent(&bar,&deleteStop);ok=ok&&bar.stops.size()==4;
        bar.resetMonochrome();ok=ok&&bar.stops.size()==2&&gradientColour(bar.stops,0)==QColor(Qt::black)&&gradientColour(bar.stops,1)==QColor(Qt::white);
        QMouseEvent keepMinimum(QEvent::MouseButtonPress,QPointF(14,60),QPointF(14,60),Qt::RightButton,Qt::RightButton,Qt::NoModifier);QApplication::sendEvent(&bar,&keepMinimum);ok=ok&&bar.stops.size()==2;
        ok=bar.setSelectedPosition(.5)&&ok;ok=ok&&bar.selectedPosition()==.5;
        ok=!bar.setSelectedPosition(1)&&ok;ok=ok&&bar.selectedPosition()==.5;
        bar.setStops(Settings().gradient);ok=ok&&bar.selectedPosition()==0;
        try{Equation::compile("z; invalid");ok=false;}catch(const std::exception &){}
        if(!ok)qWarning()<<"Playground regression check failed";
        return ok;
    }
};

int main(int argc,char **argv){
    QSurfaceFormat format;format.setVersion(4,0);format.setProfile(QSurfaceFormat::CoreProfile);format.setSamples(0);QSurfaceFormat::setDefaultFormat(format);
    QApplication app(argc,argv);app.setStyle("Fusion");
    QPalette palette;palette.setColor(QPalette::Window,QColor("#20232b"));palette.setColor(QPalette::WindowText,QColor("#e5e9f0"));palette.setColor(QPalette::Base,QColor("#151820"));palette.setColor(QPalette::AlternateBase,QColor("#252a35"));palette.setColor(QPalette::Text,QColor("#e5e9f0"));palette.setColor(QPalette::Button,QColor("#303643"));palette.setColor(QPalette::ButtonText,QColor("#e5e9f0"));palette.setColor(QPalette::Highlight,QColor("#527ec4"));palette.setColor(QPalette::HighlightedText,Qt::white);app.setPalette(palette);
    Playground window;window.show();if(app.arguments().contains("--smoke-test"))QTimer::singleShot(700,&app,[&]{app.exit(window.smoke()?0:2);});return app.exec();
}
