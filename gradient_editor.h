#pragma once
#include <QtWidgets>
#include <functional>

inline QColor gradientColour(const QJsonArray &stops,double position){
    if(stops.isEmpty())return Qt::black;
    auto first=stops.first().toObject();if(position<=first["position"].toDouble())return QColor(first["colour"].toString());
    for(int i=1;i<stops.size();++i){auto a=stops[i-1].toObject(),b=stops[i].toObject();double end=b["position"].toDouble();if(position<=end){double t=qBound(0.,(position-a["position"].toDouble())/(end-a["position"].toDouble()),1.);t=t*t*(3-2*t);QColor x(a["colour"].toString()),y(b["colour"].toString());return QColor::fromRgbF(x.redF()*(1-t)+y.redF()*t,x.greenF()*(1-t)+y.greenF()*t,x.blueF()*(1-t)+y.blueF()*t);}}
    return QColor(stops.last().toObject()["colour"].toString());
}
class GradientBar:public QWidget {
    int selected=0;bool dragging=false;
    double position(QPointF p)const{return qBound(0.,(p.x()-14)/qMax(1,width()-28),1.);}
    int nearest(QPointF p)const{int result=-1;double distance=12;for(int i=0;i<stops.size();++i){double d=std::abs(14+stops[i].toObject()["position"].toDouble()*(width()-28)-p.x());if(d<distance){distance=d;result=i;}}return result;}
    void sort(){QList<QJsonObject> values;QJsonValue chosen=stops[selected];for(auto v:stops)values<<v.toObject();std::sort(values.begin(),values.end(),[](auto a,auto b){return a["position"].toDouble()<b["position"].toDouble();});stops={};for(auto v:values)stops.append(v);for(int i=0;i<stops.size();++i)if(stops[i]==chosen){selected=i;break;}}
protected:
    void paintEvent(QPaintEvent*)override{QPainter p(this);QRect bar(14,10,width()-28,40);for(int x=0;x<bar.width();++x){p.setPen(gradientColour(stops,double(x)/qMax(1,bar.width()-1)));p.drawLine(bar.left()+x,bar.top(),bar.left()+x,bar.bottom());}p.setPen(Qt::black);p.drawRect(bar);for(int i=0;i<stops.size();++i){int x=14+qRound(stops[i].toObject()["position"].toDouble()*(width()-28));p.setPen(QPen(i==selected?Qt::white:Qt::black,2));p.setBrush(QColor(stops[i].toObject()["colour"].toString()));p.drawPolygon(QPolygon{QPoint(x,53),QPoint(x-7,65),QPoint(x+7,65)});}}
    void mousePressEvent(QMouseEvent *e)override{selected=nearest(e->position());if(selected<0){stops.append(QJsonObject{{"position",position(e->position())},{"colour",gradientColour(stops,position(e->position())).name()}});selected=stops.size()-1;sort();}dragging=true;update();if(changed)changed();}
    void mouseMoveEvent(QMouseEvent *e)override{if(!dragging||selected<0)return;double value=position(e->position());for(int i=0;i<stops.size();++i)if(i!=selected&&std::abs(stops[i].toObject()["position"].toDouble()-value)<.0001)return;auto stop=stops[selected].toObject();stop["position"]=value;stops[selected]=stop;sort();update();if(changed)changed();}
    void mouseReleaseEvent(QMouseEvent*)override{dragging=false;}
    void mouseDoubleClickEvent(QMouseEvent*)override{editColour();}
public:
    QJsonArray stops;
    std::function<void()> changed;
    explicit GradientBar(QWidget *parent=nullptr):QWidget(parent){setMinimumSize(400,80);}
    void editColour(){if(selected<0||selected>=stops.size())return;auto stop=stops[selected].toObject();QColor colour=QColorDialog::getColor(QColor(stop["colour"].toString()),this);if(colour.isValid()){stop["colour"]=colour.name();stops[selected]=stop;update();if(changed)changed();}}
    void removeSelected(){if(stops.size()<=2||selected<0)return;stops.removeAt(selected);selected=0;update();if(changed)changed();}
};
