#pragma once
#include <QtWidgets>
#include <complex>
#include <functional>
#include <limits>

class ComplexExpression {
    using C=std::complex<double>;
    QString text;int pos=0;
    std::function<C(QString)> variable;
    std::function<C(int)> history;
    void space(){while(pos<text.size()&&text[pos].isSpace())++pos;}
    bool eat(QChar ch){space();if(pos<text.size()&&text[pos]==ch){++pos;return true;}return false;}
    C sum(){C x=product();for(;;){if(eat('+'))x+=product();else if(eat('-'))x-=product();else return x;}}
    C product(){C x=unary();for(;;){if(eat('*'))x*=unary();else if(eat('/'))x/=unary();else if(eat('%')){C y=unary();if(y.real()==0||y.imag()!=0)throw std::runtime_error("Modulo requires a nonzero real divisor");x=C(x.real()-y.real()*std::floor(x.real()/y.real()),x.imag()-y.real()*std::floor(x.imag()/y.real()));}else return x;}}
    C unary(){if(eat('-'))return -unary();if(eat('+'))return unary();C x=atom();if(eat('^')){C exponent=unary();return x==C(0)&&exponent.imag()==0&&exponent.real()>0?C(0):std::pow(x,exponent);}return x;}
    C atom(){
        if(eat('(')){C x=sum();if(!eat(')'))throw std::runtime_error("Expected parenthesis");return x;}
        space();int start=pos;
        if(pos<text.size()&&(text[pos].isDigit()||text[pos]=='.')){
            while(pos<text.size()&&(text[pos].isDigit()||text[pos]=='.'))++pos;
            if(pos<text.size()&&(text[pos]=='e'||text[pos]=='E')){++pos;if(pos<text.size()&&(text[pos]=='+'||text[pos]=='-'))++pos;while(pos<text.size()&&text[pos].isDigit())++pos;}
            bool ok;double value=text.mid(start,pos-start).toDouble(&ok);if(!ok)throw std::runtime_error("Invalid number");return C(value);
        }
        while(pos<text.size()&&(text[pos].isLetterOrNumber()||text[pos]=='_'))++pos;
        QString name=text.mid(start,pos-start);
        if(name=="z"&&eat('[')){C index=sum();if(!eat(']'))throw std::runtime_error("Expected closing index bracket");double n=variable("n").real();if(index.imag()!=0||!std::isfinite(index.real())||std::floor(index.real())!=index.real())throw std::runtime_error("z index must be a finite real integer");if(index.real()<0){if(index.real()<double(std::numeric_limits<int>::min()))return C(0);return history?history(int(index.real())):C(0);}if(index.real()>n)throw std::runtime_error("Future z index exceeds n");if(index.real()<n-9)throw std::runtime_error("z index is older than the 10-value history window");if(!history)throw std::runtime_error("z history is unavailable");return history(int(index.real()));}
        if(eat('(')){C x=sum();if(!eat(')'))throw std::runtime_error("Expected parenthesis");
            if(name=="floor")return C(std::floor(x.real()),std::floor(x.imag()));
            if(name=="ceil")return C(std::ceil(x.real()),std::ceil(x.imag()));
            if(name=="sin")return std::sin(x);
            if(name=="cos")return std::cos(x);
            if(name=="exp")return std::exp(x);
            if(name=="log")return std::log(x);
            if(name=="conj")return std::conj(x);
            if(name=="abs")return C(std::abs(x.real()),std::abs(x.imag()));
            if(name=="norm")return C(std::abs(x));
            throw std::runtime_error("Unknown function");
        }
        if(name=="i")return C(0,1);
        if(name=="pi")return C(3.141592653589793);
        if(name.isEmpty())throw std::runtime_error("Expected expression");
        if(name=="z"||name=="z_n")throw std::runtime_error("Use z[index], not bare z or z_n");
        return variable(name);
    }
public:
    static C evaluate(QString text,std::function<C(QString)> variables,std::function<C(int)> history={}){ComplexExpression e;e.text=text;e.variable=variables;e.history=history;C result=e.sum();e.space();if(e.pos!=text.size())throw std::runtime_error("Unexpected expression text");return result;}
};
