#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
wf="$root/widgets/WaterfallWidget.cpp"
main="$root/main.cpp"

grep -q 'AA_EnableHighDpiScaling' "$main"
grep -q 'AA_UseHighDpiPixmaps' "$main"
[[ "$(grep -c 'devicePixelRatioF()' "$wf")" -ge 2 ]]
grep -q 'setUpdateBehavior(QOpenGLWidget::NoPartialUpdate)' "$wf"
[[ "$(grep -c 'glDisable(GL_SCISSOR_TEST)' "$wf")" -ge 2 ]]
[[ "$(grep -c 'glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE)' "$wf")" -ge 2 ]]
grep -q 'setRenderHint(QPainter::TextAntialiasing, true)' "$wf"
if grep -q 'setUpdateBehavior(QOpenGLWidget::PartialUpdate)' "$wf"; then
    echo 'ERROR: stale PartialUpdate framebuffer policy remains' >&2
    exit 1
fi
if grep -Eq 'glViewport\(0, 0, qMax\(1, width\(\)\), qMax\(1, height\(\)\)\)' "$wf"; then
    echo 'ERROR: logical-pixel GL viewport regression detected' >&2
    exit 1
fi
echo 'Waterfall HiDPI/label source audit: PASS'
