/*
 * xImJxstinn Vertical – Vorschaubild der Hochformat-Leinwand
 * Klicken = Quelle auswählen, Ziehen = verschieben, Mausrad = Größe ändern,
 * Pfeiltasten = fein verschieben (mit Shift 10 px).
 * Copyright (C) 2026 xImJxstinn – GPL-2.0-or-later
 */
#include "preview.hpp"
#include "vertical.hpp"

#include <graphics/matrix4.h>
#include <graphics/vec3.h>
#include <graphics/vec4.h>

#include <QKeyEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include <QWheelEvent>
#include <QWindow>

#include <algorithm>
#include <cmath>

/* ---------- Hilfsfunktionen ---------- */

struct FitRect {
	float x, y, scale;
};

static FitRect fit_canvas(float viewW, float viewH, float canvasW, float canvasH)
{
	FitRect r;
	r.scale = std::min(viewW / canvasW, viewH / canvasH);
	r.x = (viewW - canvasW * r.scale) * 0.5f;
	r.y = (viewH - canvasH * r.scale) * 0.5f;
	return r;
}

static obs_sceneitem_t *find_selected(obs_scene_t *scene)
{
	return XVFindItem(scene, Vertical::Get().selectedItem);
}

static void vec3_xy(vec3 &out, float x, float y, const matrix4 &m)
{
	vec3 in;
	vec3_set(&in, x, y, 0.0f);
	vec3_transform(&out, &in, &m);
}

static void draw_edge(const vec3 &a, const vec3 &b, float thickness)
{
	float dx = b.x - a.x;
	float dy = b.y - a.y;
	const float len = std::sqrt(dx * dx + dy * dy);
	if (len < 0.0001f)
		return;
	const float nx = -dy / len * thickness * 0.5f;
	const float ny = dx / len * thickness * 0.5f;
	gs_render_start(true);
	gs_vertex2f(a.x + nx, a.y + ny);
	gs_vertex2f(a.x - nx, a.y - ny);
	gs_vertex2f(b.x + nx, b.y + ny);
	gs_vertex2f(b.x - nx, b.y - ny);
	gs_render_stop(GS_TRISTRIP);
}

/* ---------- Widget ---------- */

XVPreview::XVPreview(QWidget *parent) : QWidget(parent)
{
	setAttribute(Qt::WA_PaintOnScreen);
	setAttribute(Qt::WA_StaticContents);
	setAttribute(Qt::WA_NoSystemBackground);
	setAttribute(Qt::WA_OpaquePaintEvent);
	setAttribute(Qt::WA_DontCreateNativeAncestors);
	setAttribute(Qt::WA_NativeWindow);
	setFocusPolicy(Qt::StrongFocus);
	setMouseTracking(false);
	setMinimumSize(90, 160);
}

XVPreview::~XVPreview()
{
	DestroyDisplay();
}

void XVPreview::CreateDisplay()
{
	if (display || !isVisible() || !windowHandle())
		return;
#ifdef _WIN32
	const qreal dpr = devicePixelRatioF();
	gs_init_data info = {};
	info.cx = (uint32_t)std::max(1.0, width() * dpr);
	info.cy = (uint32_t)std::max(1.0, height() * dpr);
	info.format = GS_BGRA;
	info.zsformat = GS_ZS_NONE;
	info.window.hwnd = (void *)winId();
	display = obs_display_create(&info, 0xFF14101A);
	if (display)
		obs_display_add_draw_callback(display, Draw, this);
#endif
}

void XVPreview::DestroyDisplay()
{
	if (!display)
		return;
	obs_display_remove_draw_callback(display, Draw, this);
	obs_display_destroy(display);
	display = nullptr;
}

void XVPreview::paintEvent(QPaintEvent *)
{
	CreateDisplay();
}

void XVPreview::showEvent(QShowEvent *event)
{
	QWidget::showEvent(event);
	CreateDisplay();
}

void XVPreview::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);
	CreateDisplay();
	if (display) {
		const qreal dpr = devicePixelRatioF();
		obs_display_resize(display, (uint32_t)std::max(1.0, width() * dpr),
				   (uint32_t)std::max(1.0, height() * dpr));
	}
}

bool XVPreview::event(QEvent *event)
{
	/* Wenn das Dock gelöst/angedockt wird, bekommt das Fenster eine neue ID */
	if (event->type() == QEvent::WinIdChange || event->type() == QEvent::Hide) {
		DestroyDisplay();
	}
	const bool r = QWidget::event(event);
	if (event->type() == QEvent::WinIdChange)
		CreateDisplay();
	return r;
}

bool XVPreview::ToCanvas(const QPointF &widgetPos, vec2 &out)
{
	uint32_t cx = 0, cy = 0;
	Vertical::Get().CanvasSize(cx, cy);
	if (!cx || !cy)
		return false;
	const FitRect r = fit_canvas((float)width(), (float)height(), (float)cx, (float)cy);
	if (r.scale <= 0.0f)
		return false;
	out.x = ((float)widgetPos.x() - r.x) / r.scale;
	out.y = ((float)widgetPos.y() - r.y) / r.scale;
	return true;
}

/* Oberste sichtbare, nicht gesperrte Quelle unter dem Mauszeiger */
struct HitData {
	vec2 pos;
	int64_t id;
};

static bool hit_item(obs_scene_t *, obs_sceneitem_t *item, void *param)
{
	auto hit = static_cast<HitData *>(param);
	if (!obs_sceneitem_visible(item) || obs_sceneitem_locked(item))
		return true;
	matrix4 m, inv;
	obs_sceneitem_get_box_transform(item, &m);
	if (!matrix4_inv(&inv, &m))
		return true;
	vec3 local;
	vec3_xy(local, hit->pos.x, hit->pos.y, inv);
	if (local.x >= 0.0f && local.x <= 1.0f && local.y >= 0.0f && local.y <= 1.0f)
		hit->id = obs_sceneitem_get_id(item);
	return true;
}

void XVPreview::mousePressEvent(QMouseEvent *event)
{
	setFocus();
	if (event->button() != Qt::LeftButton)
		return;
	vec2 pos;
	if (!ToCanvas(event->position(), pos))
		return;

	Vertical &v = Vertical::Get();
	obs_source_t *src = v.CurrentSceneRef();
	obs_scene_t *scene = obs_scene_from_source(src);
	HitData hit = {pos, 0};
	if (scene)
		obs_scene_enum_items(scene, hit_item, &hit);

	/* Ist schon eine Quelle in einer Gruppe ausgewählt und liegt der Klick darauf, bleibt sie ausgewählt */
	if (obs_sceneitem_t *sel = find_selected(scene)) {
		if (obs_sceneitem_get_group(scene, sel) && !obs_sceneitem_locked(sel)) {
			matrix4 box, inv;
			XVItemBox(scene, sel, box);
			vec3 local;
			if (matrix4_inv(&inv, &box)) {
				vec3_xy(local, pos.x, pos.y, inv);
				if (local.x >= 0.0f && local.x <= 1.0f && local.y >= 0.0f && local.y <= 1.0f)
					hit.id = obs_sceneitem_get_id(sel);
			}
		}
	}

	v.selectedItem = hit.id;
	dragging = false;
	if (obs_sceneitem_t *item = find_selected(scene)) {
		obs_sceneitem_get_pos(item, &itemStart);
		XVItemParentInverse(scene, item, parentInv);
		vec3 local;
		vec3_xy(local, pos.x, pos.y, parentInv);
		vec2_set(&dragStart, local.x, local.y);
		dragging = true;
	}
	obs_source_release(src);
	v.Notify();
}

void XVPreview::mouseMoveEvent(QMouseEvent *event)
{
	if (!dragging)
		return;
	vec2 pos;
	if (!ToCanvas(event->position(), pos))
		return;
	obs_source_t *src = Vertical::Get().CurrentSceneRef();
	if (obs_sceneitem_t *item = find_selected(obs_scene_from_source(src))) {
		vec3 local;
		vec3_xy(local, pos.x, pos.y, parentInv);
		vec2 np;
		np.x = std::round(itemStart.x + (local.x - dragStart.x));
		np.y = std::round(itemStart.y + (local.y - dragStart.y));
		obs_sceneitem_set_pos(item, &np);
	}
	obs_source_release(src);
}

void XVPreview::mouseReleaseEvent(QMouseEvent *)
{
	dragging = false;
}

void XVPreview::wheelEvent(QWheelEvent *event)
{
	obs_source_t *src = Vertical::Get().CurrentSceneRef();
	obs_scene_t *wheelScene = obs_scene_from_source(src);
	obs_sceneitem_t *item = find_selected(wheelScene);
	if (item) {
		const float steps = (float)event->angleDelta().y() / 120.0f;
		const float factor = std::pow(1.08f, steps);

		matrix4 m, inv;
		vec3 before, after;
		XVItemBox(wheelScene, item, m);
		vec3_xy(before, 0.5f, 0.5f, m);

		if (obs_sceneitem_get_bounds_type(item) != OBS_BOUNDS_NONE) {
			vec2 b;
			obs_sceneitem_get_bounds(item, &b);
			b.x = std::max(8.0f, b.x * factor);
			b.y = std::max(8.0f, b.y * factor);
			obs_sceneitem_set_bounds(item, &b);
		} else {
			vec2 s;
			obs_sceneitem_get_scale(item, &s);
			s.x = std::max(0.01f, s.x * factor);
			s.y = std::max(0.01f, s.y * factor);
			obs_sceneitem_set_scale(item, &s);
		}
		obs_sceneitem_force_update_transform(item);
		XVItemBox(wheelScene, item, m);
		vec3_xy(after, 0.5f, 0.5f, m);

		/* Verschiebung in Gruppen-Koordinaten umrechnen */
		XVItemParentInverse(wheelScene, item, inv);
		vec3 lb, la;
		vec3_xy(lb, before.x, before.y, inv);
		vec3_xy(la, after.x, after.y, inv);

		vec2 p;
		obs_sceneitem_get_pos(item, &p);
		p.x += lb.x - la.x;
		p.y += lb.y - la.y;
		obs_sceneitem_set_pos(item, &p);
		event->accept();
	}
	obs_source_release(src);
}

void XVPreview::keyPressEvent(QKeyEvent *event)
{
	const float step = (event->modifiers() & Qt::ShiftModifier) ? 10.0f : 1.0f;
	float dx = 0.0f, dy = 0.0f;
	switch (event->key()) {
	case Qt::Key_Left:
		dx = -step;
		break;
	case Qt::Key_Right:
		dx = step;
		break;
	case Qt::Key_Up:
		dy = -step;
		break;
	case Qt::Key_Down:
		dy = step;
		break;
	default:
		QWidget::keyPressEvent(event);
		return;
	}
	obs_source_t *src = Vertical::Get().CurrentSceneRef();
	if (obs_sceneitem_t *item = find_selected(obs_scene_from_source(src))) {
		vec2 p;
		obs_sceneitem_get_pos(item, &p);
		p.x += dx;
		p.y += dy;
		obs_sceneitem_set_pos(item, &p);
	}
	obs_source_release(src);
}

/* ---------- Zeichnen (Grafik-Thread) ---------- */

void XVPreview::Draw(void *, uint32_t cx, uint32_t cy)
{
	Vertical &v = Vertical::Get();
	obs_canvas_t *canvas = v.CanvasRef();
	if (!canvas)
		return;
	obs_video_info ovi;
	if (!obs_canvas_get_video_info(canvas, &ovi) || !ovi.base_width || !ovi.base_height) {
		obs_canvas_release(canvas);
		return;
	}

	const float bw = (float)ovi.base_width;
	const float bh = (float)ovi.base_height;
	const FitRect r = fit_canvas((float)cx, (float)cy, bw, bh);

	gs_viewport_push();
	gs_projection_push();
	gs_ortho(0.0f, bw, 0.0f, bh, -100.0f, 100.0f);
	gs_set_viewport((int)r.x, (int)r.y, (int)(bw * r.scale), (int)(bh * r.scale));

	obs_render_canvas_texture(canvas);

	/* Rahmen um die ausgewählte Quelle */
	obs_source_t *src = v.CurrentSceneRef();
	obs_scene_t *drawScene = obs_scene_from_source(src);
	obs_sceneitem_t *item = find_selected(drawScene);
	if (item && obs_sceneitem_visible(item)) {
		matrix4 m;
		XVItemBox(drawScene, item, m);
		vec3 c[4];
		vec3_xy(c[0], 0.0f, 0.0f, m);
		vec3_xy(c[1], 1.0f, 0.0f, m);
		vec3_xy(c[2], 1.0f, 1.0f, m);
		vec3_xy(c[3], 0.0f, 1.0f, m);

		gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
		gs_eparam_t *color = gs_effect_get_param_by_name(solid, "color");
		vec4 col;
		vec4_set(&col, 1.0f, 0.82f, 0.25f, 1.0f);
		gs_effect_set_vec4(color, &col);
		const float thickness = 3.0f / std::max(r.scale, 0.01f);
		while (gs_effect_loop(solid, "Solid")) {
			for (int i = 0; i < 4; i++)
				draw_edge(c[i], c[(i + 1) % 4], thickness);
		}
	}
	obs_source_release(src);

	gs_projection_pop();
	gs_viewport_pop();
	obs_canvas_release(canvas);
}
