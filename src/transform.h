#ifndef TRANSFORM_H
#define TRANSFORM_H

#include <cmath>
#include <utility>

class Transform2D {
public:
    Transform2D() : m_a(1.0), m_b(0.0), m_c(0.0), m_d(1.0), m_tx(0.0), m_ty(0.0) {}

    static Transform2D identity() { return Transform2D(); }

    static Transform2D fromMatrix(double a, double b, double c, double d, double tx, double ty) {
        Transform2D t;
        t.m_a = a;
        t.m_b = b;
        t.m_c = c;
        t.m_d = d;
        t.m_tx = tx;
        t.m_ty = ty;
        return t;
    }

    static Transform2D translation(double dx, double dy) {
        Transform2D t;
        t.m_tx = dx;
        t.m_ty = dy;
        return t;
    }

    static Transform2D rotationRadians(double radians, double pivotX = 0.0, double pivotY = 0.0) {
        Transform2D t;
        t.rotateRadians(radians, pivotX, pivotY);
        return t;
    }

    static Transform2D scaling(double sx, double sy, double pivotX = 0.0, double pivotY = 0.0) {
        Transform2D t;
        t.scale(sx, sy, pivotX, pivotY);
        return t;
    }

    static Transform2D shearing(double shx, double shy, double pivotX = 0.0, double pivotY = 0.0) {
        Transform2D t;
        t.shear(shx, shy, pivotX, pivotY);
        return t;
    }

    Transform2D& setIdentity() {
        m_a = 1.0;
        m_b = 0.0;
        m_c = 0.0;
        m_d = 1.0;
        m_tx = 0.0;
        m_ty = 0.0;
        return *this;
    }

    Transform2D& setTranslation(double dx, double dy) {
        setIdentity();
        m_tx = dx;
        m_ty = dy;
        return *this;
    }

    Transform2D& setRotationRadians(double radians, double pivotX = 0.0, double pivotY = 0.0) {
        setIdentity();
        return rotateRadians(radians, pivotX, pivotY);
    }

    Transform2D& setRotationDegrees(double degrees, double pivotX = 0.0, double pivotY = 0.0) {
        return setRotationRadians(degreesToRadians(degrees), pivotX, pivotY);
    }

    Transform2D& setScale(double sx, double sy, double pivotX = 0.0, double pivotY = 0.0) {
        setIdentity();
        return scale(sx, sy, pivotX, pivotY);
    }

    Transform2D& setShear(double shx, double shy, double pivotX = 0.0, double pivotY = 0.0) {
        setIdentity();
        return shear(shx, shy, pivotX, pivotY);
    }

    Transform2D& translate(double dx, double dy) {
        return (*this) *= translation(dx, dy);
    }

    Transform2D& rotateRadians(double radians, double pivotX = 0.0, double pivotY = 0.0) {
        const double cosR = std::cos(radians);
        const double sinR = std::sin(radians);
        Transform2D rot = fromMatrix(cosR, sinR, -sinR, cosR, 0.0, 0.0);
        if (pivotX != 0.0 || pivotY != 0.0) {
            Transform2D pre = translation(pivotX, pivotY);
            Transform2D post = translation(-pivotX, -pivotY);
            (*this) *= pre;
            (*this) *= rot;
            (*this) *= post;
            return *this;
        }
        return (*this) *= rot;
    }

    Transform2D& rotateDegrees(double degrees, double pivotX = 0.0, double pivotY = 0.0) {
        return rotateRadians(degreesToRadians(degrees), pivotX, pivotY);
    }

    Transform2D& scale(double sx, double sy, double pivotX = 0.0, double pivotY = 0.0) {
        Transform2D sc = fromMatrix(sx, 0.0, 0.0, sy, 0.0, 0.0);
        if (pivotX != 0.0 || pivotY != 0.0) {
            Transform2D pre = translation(pivotX, pivotY);
            Transform2D post = translation(-pivotX, -pivotY);
            (*this) *= pre;
            (*this) *= sc;
            (*this) *= post;
            return *this;
        }
        return (*this) *= sc;
    }

    Transform2D& shear(double shx, double shy, double pivotX = 0.0, double pivotY = 0.0) {
        Transform2D sh = fromMatrix(1.0, shy, shx, 1.0, 0.0, 0.0);
        if (pivotX != 0.0 || pivotY != 0.0) {
            Transform2D pre = translation(pivotX, pivotY);
            Transform2D post = translation(-pivotX, -pivotY);
            (*this) *= pre;
            (*this) *= sh;
            (*this) *= post;
            return *this;
        }
        return (*this) *= sh;
    }

    bool isIdentity(double eps = 1e-9) const {
        return std::abs(m_a - 1.0) <= eps && std::abs(m_d - 1.0) <= eps &&
               std::abs(m_b) <= eps && std::abs(m_c) <= eps &&
               std::abs(m_tx) <= eps && std::abs(m_ty) <= eps;
    }

    double a() const { return m_a; }
    double b() const { return m_b; }
    double c() const { return m_c; }
    double d() const { return m_d; }
    double tx() const { return m_tx; }
    double ty() const { return m_ty; }

    Transform2D operator*(const Transform2D& other) const {
        Transform2D out;
        out.m_a = m_a * other.m_a + m_c * other.m_b;
        out.m_b = m_b * other.m_a + m_d * other.m_b;
        out.m_c = m_a * other.m_c + m_c * other.m_d;
        out.m_d = m_b * other.m_c + m_d * other.m_d;
        out.m_tx = m_a * other.m_tx + m_c * other.m_ty + m_tx;
        out.m_ty = m_b * other.m_tx + m_d * other.m_ty + m_ty;
        return out;
    }

    Transform2D& operator*=(const Transform2D& other) {
        *this = (*this) * other;
        return *this;
    }

    std::pair<double, double> apply(double x, double y) const {
        return {m_a * x + m_c * y + m_tx, m_b * x + m_d * y + m_ty};
    }

    std::pair<double, double> applyInverse(double x, double y) const {
        const double det = m_a * m_d - m_b * m_c;
        if (std::abs(det) <= 1e-12) {
            return {x, y};
        }
        const double invA = m_d / det;
        const double invB = -m_b / det;
        const double invC = -m_c / det;
        const double invD = m_a / det;
        const double invTx = -(invA * m_tx + invC * m_ty);
        const double invTy = -(invB * m_tx + invD * m_ty);
        return {invA * x + invC * y + invTx, invB * x + invD * y + invTy};
    }

private:
    static double degreesToRadians(double degrees) {
        return degrees * 3.14159265358979323846 / 180.0;
    }

    double m_a;
    double m_b;
    double m_c;
    double m_d;
    double m_tx;
    double m_ty;
};

class Transformable {
public:
    Transform2D& transform() { return m_transform; }
    const Transform2D& transform() const { return m_transform; }

private:
    Transform2D m_transform;
};

#endif
