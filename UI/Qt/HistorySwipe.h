/*
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <QWheelEvent>

namespace Ladybird {

class HistorySwipe {
public:
    enum class Result { Pass, Consume, Back, Forward };

    Result update(QWheelEvent const& event, int width, QPointF delta, bool can_go_back, bool can_go_forward)
    {
        auto phase = event.phase();
        auto time = event.timestamp();
        if (phase == Qt::ScrollMomentum)
            return m_claimed ? Result::Consume : Result::Pass;
        if (phase == Qt::ScrollEnd) {
            m_tracking = false;
            return m_claimed ? Result::Consume : Result::Pass;
        }
        if (phase == Qt::ScrollBegin || !m_tracking || (phase == Qt::NoScrollPhase && time - m_last_time > 250)) {
            m_tracking = true;
            m_claimed = false;
            m_finished = false;
            m_distance = 0;
            auto x = event.position().x();
            auto edge = qMin(42.0, width / 6.0);
            m_direction = x <= edge && can_go_back ? -1 : x >= width - edge && can_go_forward ? 1 : 0;
        }
        m_last_time = time;
        if (!m_direction || delta.isNull())
            return m_claimed ? Result::Consume : Result::Pass;
        if (!m_claimed && (qAbs(delta.x()) <= 1.5 * qAbs(delta.y()) || delta.x() * m_direction <= 0)) {
            m_direction = 0;
            return Result::Pass;
        }
        m_claimed = true;
        m_distance += delta.x() * m_direction;
        if (m_finished || m_distance < 90)
            return Result::Consume;
        m_finished = true;
        return m_direction < 0 ? Result::Back : Result::Forward;
    }

private:
    quint64 m_last_time { 0 };
    double m_distance { 0 };
    int m_direction { 0 };
    bool m_tracking { false };
    bool m_claimed { false };
    bool m_finished { false };
};

}
