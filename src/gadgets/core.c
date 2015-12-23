#include "e_mod_main.h"
#include "gadget.h"

#define ZGS_IS_HORIZ(gravity) \
  ((gravity) == Z_GADGET_SITE_GRAVITY_LEFT) || ((gravity) == Z_GADGET_SITE_GRAVITY_RIGHT)

#define ZGS_GET(obj) \
   zgs = evas_object_data_get((obj), "__z_gadget_site"); \
   if (!zgs) abort()

typedef struct Z_Gadget_Config Z_Gadget_Config;

typedef struct Z_Gadget_Site
{
   Evas_Object *layout;
   Evas_Object *events;
   Z_Gadget_Site_Gravity gravity;
   Eina_List *gadgets;
   int cur_size;

   Z_Gadget_Config *action;
   Ecore_Event_Handler *move_handler;
   Ecore_Event_Handler *mouse_up_handler;
   int button;
} Z_Gadget_Site;


/* refcount? */
struct Z_Gadget_Config
{
   E_Object *e_obj_inherit; //list?
   Evas_Object *gadget; //list?
   unsigned int id;
   Eina_Stringshare *type;
   Z_Gadget_Site *site;

   double x, y; //fixed % positioning
   Evas_Point offset; //offset from mouse down
   Z_Gadget_Config *over; //gadget is animating over another gadget during drag
   Eina_Bool modifying : 1;
};

static Eina_Hash *gadget_types;

static Eina_List *sites;

static E_Action *move_act;

static Z_Gadget_Config *
_gadget_at_xy(Z_Gadget_Site *zgs, int x, int y)
{
   Eina_List *l;
   Z_Gadget_Config *zgc;
   Evas_Object *win;
   int wx, wy;

   win = e_win_evas_object_win_get(zgs->layout);
   evas_object_geometry_get(win, &wx, &wy, NULL, NULL);
   EINA_LIST_FOREACH(zgs->gadgets, l, zgc)
     {
        int ox, oy, ow, oh;

        if (!zgc->gadget) continue;

        evas_object_geometry_get(zgc->gadget, &ox, &oy, &ow, &oh);
        if (E_INSIDE(x, y, ox + wx, oy + wy, ow, oh)) return zgc;
     }
   return NULL;
}

static void
_gravity_apply(Evas_Object *ly, Z_Gadget_Site_Gravity gravity)
{
   elm_box_horizontal_set(ly, ZGS_IS_HORIZ(gravity));
   elm_box_align_set(ly,
     ZGS_IS_HORIZ(gravity) ? gravity - Z_GADGET_SITE_GRAVITY_LEFT : 0.5,
     ZGS_IS_HORIZ(gravity) ? 0.5 : gravity - Z_GADGET_SITE_GRAVITY_TOP);
}

static void
_gadget_reparent(Z_Gadget_Site *zgs, Evas_Object *g)
{
   switch (zgs->gravity)
     {
      case Z_GADGET_SITE_GRAVITY_NONE:
        /* fake */
        break;
      case Z_GADGET_SITE_GRAVITY_LEFT:
      case Z_GADGET_SITE_GRAVITY_TOP:
        elm_box_pack_end(zgs->layout, g);
        break;
      default:
        /* right aligned: pack on left */
          elm_box_pack_start(zgs->layout, g);
     }
}

static void
_gadget_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Z_Gadget_Config *zgc = data;

   if (!e_object_is_del(zgc->e_obj_inherit))
     e_object_del(zgc->e_obj_inherit);
}

static void
_gadget_object_free(E_Object *eobj)
{
   Z_Gadget_Config *zgc;
   Evas_Object *g;

   g = e_object_data_get(eobj);
   zgc = evas_object_data_get(g, "__z_gadget");
   evas_object_smart_callback_call(zgc->site->layout, "gadget_removed", zgc->gadget);
   E_FREE_FUNC(zgc->gadget, evas_object_del);
   E_FREE(zgc->e_obj_inherit);
}

static void
_site_extents_calc(Evas_Object *box, Evas_Object_Box_Data *priv, Eina_Bool horizontal)
{
   Evas_Coord minw, minh, mnw, mnh, maxw, maxh;
   Evas_Coord *rw, *rh, *rminw, *rminh, *rmaxw, *rmaxh;
   const Eina_List *l;
   Evas_Object_Box_Option *opt;
   Eina_Bool max = EINA_TRUE;

   minw = 0;
   minh = 0;
   maxw = -1;
   maxh = -1;

   /* calculate but after switched w and h for horizontal mode */
   if (!horizontal)
     {
        /* use pointer for real w/h to allow size hint calcs to run only once
         * for both orientations
         */
        rw = &mnw;
        rh = &mnh;
        rminw = &minw;
        rminh = &minh;
        rmaxw = &maxw;
        rmaxh = &maxh;
     }
   else
     {
        rw = &mnh;
        rh = &mnw;
        rminw = &minh;
        rminh = &minw;
        rmaxw = &maxh;
        rmaxh = &maxw;
     }
   EINA_LIST_FOREACH(priv->children, l, opt)
     {
        evas_object_size_hint_min_get(opt->obj, &mnw, &mnh);
        if (*rminw < *rw) *rminw = *rw;
        *rminh += *rh;

        /* FIXME: worthwhile to allow object padding? */

        evas_object_size_hint_max_get(opt->obj, &mnw, &mnh);
        if (*rh < 0)
          {
             *rmaxh = -1;
             max = EINA_FALSE;
          }
        if (max) *rmaxh += *rh;

        if (*rw >= 0)
          {
             if (*rmaxw == -1) *rmaxw = *rw;
             else if (*rmaxw > *rw) *rmaxw = *rw;
          }
     }
   if ((maxw >= 0) && (minw > maxw)) maxw = minw;
   if ((maxh >= 0) && (minh > maxh)) maxh = minh;
   evas_object_size_hint_min_set(box, minw, minh);
   evas_object_size_hint_max_set(box, maxw, maxh);
}

static void
_site_gadget_resize(Evas_Object *g, int w, int h, Evas_Coord *ww, Evas_Coord *hh, Evas_Coord *ow, Evas_Coord *oh)
{
   Evas_Coord mnw, mnh, mxw, mxh;
   Z_Gadget_Config *zgc;

   zgc = evas_object_data_get(g, "__z_gadget");

   evas_object_size_hint_min_get(g, &mnw, &mnh);
   evas_object_size_hint_max_get(g, &mxw, &mxh);

   /* TODO: aspect */
   if (ZGS_IS_HORIZ(zgc->site->gravity))
     {
        *ww = mnw, *hh = h;
        if (!(*ww)) *ww = *hh;
     }
   else
     {
        *hh = mnh, *ww = w;
        if (!(*hh)) *hh = *ww;
     }
   *ow = *ww;
   if ((mxw >= 0) && (mxw < *ow)) *ow = mxw;
   *oh = *hh;
   if ((mxh >= 0) && (mxh < *oh)) *oh = mxh;

   evas_object_resize(g, *ow, *oh);
}

static void
_site_layout(Evas_Object *o, Evas_Object_Box_Data *priv, void *data)
{
   Z_Gadget_Site *zgs = data;
   Evas_Coord x, y, w, h, xx, yy, px, py;
   Eina_List *l, *fixed = NULL;
   Evas_Coord minw, minh;
   double ax, ay;
   Z_Gadget_Config *zgc;

   _site_extents_calc(o, priv, ZGS_IS_HORIZ(zgs->gravity));

   evas_object_geometry_get(o, &x, &y, &w, &h);
   evas_object_geometry_set(zgs->events, x, y, w, h);

   evas_object_size_hint_min_get(o, &minw, &minh);
   evas_object_box_align_get(o, &ax, &ay);
   if (w < minw)
     {
        x = x + ((w - minw) * (1.0 - ax));
        w = minw;
     }
   if (h < minh)
     {
        y = y + ((h - minh) * (1.0 - ay));
        h = minh;
     }

   px = xx = x;
   py = yy = y;

   /* do layout for rest of gadgets now to avoid fixed gadgets */
   if (zgs->gravity % 2)//left/top
     {
        EINA_LIST_FOREACH(zgs->gadgets, l, zgc)
          {
             Evas_Coord gx = xx, gy = yy;
             int ww, hh, ow, oh;

             if ((zgc->x > -1) || (zgc->y > -1)) break; //one fixed gadget reached
             _site_gadget_resize(zgc->gadget, w, h, &ww, &hh, &ow, &oh);
             gx += (Evas_Coord)(((double)(ww - ow)) * 0.5);
             gy += (Evas_Coord)(((double)(hh - oh)) * 0.5);
             evas_object_move(zgc->gadget, gx, gy);
             if (ZGS_IS_HORIZ(zgs->gravity))
               xx += ww;
             else
               yy += hh;
          }
     }
   else
     {
        EINA_LIST_REVERSE_FOREACH(zgs->gadgets, l, zgc)
          {
             Evas_Coord gx = xx, gy = yy;
             int ww, hh, ow, oh;

             if ((zgc->x > -1) || (zgc->y > -1)) continue; //one fixed gadget reached
             _site_gadget_resize(zgc->gadget, w, h, &ww, &hh, &ow, &oh);
             gx += (Evas_Coord)(((double)(ww - ow)) * 0.5);
             gy += (Evas_Coord)(((double)(hh - oh)) * 0.5);
             evas_object_move(zgc->gadget, gx, gy);
             if (ZGS_IS_HORIZ(zgs->gravity))
               xx += ww;
             else
               yy += hh;
          }
     }
   px = xx;
   py = yy;

   if (ZGS_IS_HORIZ(zgs->gravity))
     zgs->cur_size = px;
   else
     zgs->cur_size = py;

   /* do layout for fixed position gadgets after */
   EINA_LIST_REVERSE_FOREACH(zgs->gadgets, l, zgc)
     {
        Evas_Coord gx = xx, gy = yy;
        int ww, hh, ow, oh;

        if ((zgc->x < 0) && (zgc->y < 0)) break; //once non-fixed gadget reached
        _site_gadget_resize(zgc->gadget, w, h, &ww, &hh, &ow, &oh);
        fixed = eina_list_append(fixed, zgc);
        if (ZGS_IS_HORIZ(zgc->site->gravity))
          gx = zgc->x * (xx + MAX(w, minw));
        else
          gy = zgc->y * (yy + MAX(h, minh));
        gx += (Evas_Coord)(((double)(ww - ow)) * 0.5);
        gy += (Evas_Coord)(((double)(hh - oh)) * 0.5);
        if (gx < px) gx = px;
        if (gy < py) gy = py;
        
        evas_object_move(zgc->gadget, gx, gy);
        if (ZGS_IS_HORIZ(zgs->gravity))
          px = gx + ww;
        else
          py = gy + hh;
     }


   eina_list_free(fixed);
}

static int
_site_gadgets_sort(Z_Gadget_Config *a, Z_Gadget_Config *b)
{
   double *ax, *bx;
   if (ZGS_IS_HORIZ(a->site->gravity))
     ax = &a->x, bx = &b->x;
   else
     ax = &a->y, bx = &b->y;
   if (a->site->gravity % 2)//left/top
     return lround(*ax - *bx);
   return lround(*bx - *ax);
}

static Eina_Bool
_gadget_mouse_move(Z_Gadget_Config *zgc, int t EINA_UNUSED, Ecore_Event_Mouse_Move *ev)
{
   int wx, wy;//window coords
   int x, y, w, h;//site geom
   int mx, my;//mouse coords normalized for layout orientation
   int gw, gh;//"relative" region size
   int *rw, *rh;//"relative" region size aliasing
   int ox, oy, ow, oh;//gadget geom
   Eina_List *fixed;
   Z_Gadget_Config *z;
   Evas_Object *win;

   /* adjust window -> screen coords */
   win = e_win_evas_object_win_get(zgc->site->layout);
   evas_object_geometry_get(win, &wx, &wy, NULL, NULL);
   evas_object_geometry_get(zgc->site->layout, &x, &y, &w, &h);
   x += wx, y += wy;
   gw = w, gh = h;

   mx = ev->x;
   my = ev->y;

   evas_object_geometry_get(zgc->gadget, &ox, &oy, &ow, &oh);
   ox += wx, oy += wy;

   rw = &gw;
   rh = &gh;
   /* normalize constrained axis to get a valid coordinate */
   if (ZGS_IS_HORIZ(zgc->site->gravity))
     {
        my = y + 1;
        *rw = zgc->site->cur_size;
     }
   else
     {
        mx = x + 1;
        *rh = zgc->site->cur_size;
     }

   /* find first "fixed" position gadget for later use */
   EINA_LIST_FOREACH(zgc->site->gadgets, fixed, z)
     if ((z->x > -1) || (z->y > -1)) break;

   if (E_INSIDE(mx, my, x, y, w, h))
     {
        /* dragging inside site */
        int sx = x, sy = y;
        double ax, ay;

        /* adjust contiguous site geometry for gravity */
        elm_box_align_get(zgc->site->layout, &ax, &ay);
        if (ZGS_IS_HORIZ(zgc->site->gravity))
          sx = x + ((w - zgc->site->cur_size) * (1.0 - ax));
        else
          sy = y + ((h - zgc->site->cur_size) * (1.0 - ay));
        if (E_INSIDE(mx, my, sx, sy, *rw, *rh))
          {
             /* dragging inside relative area */
             Eina_List *l;

             EINA_LIST_FOREACH(zgc->site->gadgets, l, z)
               {
                  int ggx, ggy, ggw, ggh;
                  Eina_Bool left;//moving gadget is "left" of this gadget
                  int *pmx, *pggx, *pggw, *pwx, *pw;
                  double *zx;

                  if (z == zgc) continue;
                  if (!z->gadget) continue; //no gadget object for this config

                  evas_object_geometry_get(z->gadget, &ggx, &ggy, &ggw, &ggh);
                  ggx += wx, ggy += wy;
                  /* not inside this gadget! */
                  if (!E_INSIDE(mx, my, ggx, ggy, ggw, ggh)) continue;

                  if (ZGS_IS_HORIZ(zgc->site->gravity))
                    left = ox < ggx;
                  else
                    left = oy < ggy;

                  if (ZGS_IS_HORIZ(zgc->site->gravity))
                    pmx = &mx, pggx = &ggx, pggw = &ggw, pwx = &wx, pw = &w, zx = &zgc->x;
                  else
                    pmx = &my, pggx = &ggy, pggw = &ggh, pwx = &wy, pw = &h, zx = &zgc->y;
                  if (left)
                    {
                       if (*pmx > *pggx + (*pggw / 2)) // more than halfway over
                         {
                            zgc->site->gadgets = eina_list_remove(zgc->site->gadgets, zgc);
                            zgc->site->gadgets = eina_list_append_relative_list(zgc->site->gadgets, zgc, l);
                            zgc->over = NULL;
                            *zx = -1.0;
                         }
                       else // less
                         {
                            *zx = (double)(*pmx - *pwx) / (double)*pw;
                            zgc->over = z;
                         }
                    }
                  else
                    {
                       if (*pmx < *pggx + (*pggw / 2)) // more than halfway over
                         {
                            zgc->site->gadgets = eina_list_remove(zgc->site->gadgets, zgc);
                            zgc->site->gadgets = eina_list_prepend_relative_list(zgc->site->gadgets, zgc, l);
                            zgc->over = NULL;
                            *zx = -1.0;
                         }
                       else // less
                         {
                            *zx = (double)(*pmx - *pwx) / (double)*pw;
                            zgc->over = z;
                         }
                    }
               }
          }
        else
          {
             /* dragging outside relative area */
             if (fixed)
               {
                  Eina_List *l;
                  Z_Gadget_Config *zz;

                  for (l = fixed; l; l = l->next)
                    {
                       int zx, zy, zw, zh;

                       zz = eina_list_data_get(l);

                       if ((zz = zgc) || (!zz->gadget)) continue;

                       /* FIXME: shortcut */
                       evas_object_geometry_get(zz->gadget, &zx, &zy, &zw, &zh);
                       if (E_INSIDE(mx, my, zx, zy, zw, zh))
                         {
                            zgc->over = zz;
                            break;
                         }
                    }
               }
             if (!((zgc->x > -1) || (zgc->y > -1)))
               {
                  zgc->site->gadgets = eina_list_remove(zgc->site->gadgets, zgc);
                  zgc->site->gadgets = eina_list_append(zgc->site->gadgets, zgc);
               }
             if (ZGS_IS_HORIZ(zgc->site->gravity))
               zgc->x = (double)(mx - wx - zgc->offset.x) / (double)w;
             else
               zgc->y = (double)(my - wy - zgc->offset.y) / (double)h;
          }
     }
   else
     {
        /* dragging to edge of site */
        Eina_Bool left;
        double *fx;

        if (ZGS_IS_HORIZ(zgc->site->gravity))
          {
             fx = &zgc->x;
             left = mx <= x;
          }
        else
          {
             fx = &zgc->y;
             left = my <= y;
          }
        if (left)
          {
             if ((zgc->site->gravity == Z_GADGET_SITE_GRAVITY_LEFT) || (zgc->site->gravity == Z_GADGET_SITE_GRAVITY_TOP))
               {
                  *fx = -1.0;
                  zgc->site->gadgets = eina_list_promote_list(zgc->site->gadgets, eina_list_data_find_list(zgc->site->gadgets, zgc));
               }
             else
               {
                  *fx = 0.0;
                  zgc->site->gadgets = eina_list_remove(zgc->site->gadgets, zgc);
                  zgc->site->gadgets = eina_list_prepend_relative_list(zgc->site->gadgets, zgc, fixed);
               }
          }
        else
          {
             if ((zgc->site->gravity == Z_GADGET_SITE_GRAVITY_LEFT) || (zgc->site->gravity == Z_GADGET_SITE_GRAVITY_TOP))
               {
                  *fx = 1.0;
                  zgc->site->gadgets = eina_list_demote_list(zgc->site->gadgets, eina_list_data_find_list(zgc->site->gadgets, zgc));
               }
             else
               {
                  *fx = -1.0;
                  zgc->site->gadgets = eina_list_remove(zgc->site->gadgets, zgc);
                  zgc->site->gadgets = eina_list_prepend_relative_list(zgc->site->gadgets, zgc, fixed);
               }
          }
     }
   elm_box_recalculate(zgc->site->layout);

   return ECORE_CALLBACK_RENEW;
}

static void
_gadget_act_modify_end(E_Object *obj, const char *params EINA_UNUSED, E_Binding_Event_Mouse_Button *ev EINA_UNUSED)
{
   Z_Gadget_Config *zgc;
   Evas_Object *g;

   g = e_object_data_get(obj);
   zgc = evas_object_data_get(g, "__z_gadget");
   zgc->modifying = 0;
   if (zgc->over)
     {
        /* FIXME: animate */
        zgc->x = zgc->y = -1.0;
        evas_object_smart_need_recalculate_set(zgc->site->layout, 1);
     }
   zgc->over = NULL;

   E_FREE_FUNC(zgc->site->move_handler, ecore_event_handler_del);
}

static void
_gadget_act_modify(E_Object *obj, const char *params EINA_UNUSED, E_Binding_Event_Mouse_Button *ev EINA_UNUSED)
{
   Z_Gadget_Config *zgc;
   Evas_Object *g;

   if (obj->type != Z_GADGET_TYPE) return;

   g = e_object_data_get(obj);
   zgc = evas_object_data_get(g, "__z_gadget");
   zgc->modifying = 1;
   if (!zgc->site->move_handler)
     zgc->site->move_handler = ecore_event_handler_add(ECORE_EVENT_MOUSE_MOVE, (Ecore_Event_Handler_Cb)_gadget_mouse_move, zgc);
}

static Eina_Bool
_site_mouse_up(Z_Gadget_Site *zgs, int t EINA_UNUSED, Ecore_Event_Mouse_Button *ev)
{
   if (e_bindings_mouse_up_ecore_event_handle(E_BINDING_CONTEXT_ANY, zgs->action->e_obj_inherit, ev))
     {
        evas_object_pointer_mode_set(zgs->events, EVAS_OBJECT_POINTER_MODE_NOGRAB);
        zgs->action = NULL;
        E_FREE_FUNC(zgs->mouse_up_handler, ecore_event_handler_del);
     }
   return ECORE_CALLBACK_RENEW;
}

static void
_site_mouse_down(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info)
{
   Z_Gadget_Site *zgs = data;
   Evas_Event_Mouse_Down *ev = event_info;
   Z_Gadget_Config *zgc;

   zgc = _gadget_at_xy(zgs, ev->output.x, ev->output.y);
   if (!zgc) return;
   if (e_bindings_mouse_down_evas_event_handle(E_BINDING_CONTEXT_ANY, zgc->e_obj_inherit, event_info))
     {
        int x, y;

        evas_object_pointer_mode_set(obj, EVAS_OBJECT_POINTER_MODE_NOGRAB_NO_REPEAT_UPDOWN);
        zgs->action = zgc;
        if (!zgs->mouse_up_handler)
          zgs->mouse_up_handler = ecore_event_handler_add(ECORE_EVENT_MOUSE_BUTTON_UP, (Ecore_Event_Handler_Cb)_site_mouse_up, zgs);


        evas_object_geometry_get(zgc->gadget, &x, &y, NULL, NULL);
        zgc->offset.x = ev->canvas.x - x;
        zgc->offset.y = ev->canvas.y - y;
     }
}

Z_API Evas_Object *
z_gadget_site_add(Evas_Object *parent, Z_Gadget_Site_Gravity gravity)
{
   Z_Gadget_Site *zgs;

   zgs = E_NEW(Z_Gadget_Site, 1);

   zgs->gravity = gravity;
   if (gravity)
     {
        zgs->layout = elm_box_add(parent);
        _gravity_apply(zgs->layout, gravity);
        elm_box_layout_set(zgs->layout, _site_layout, zgs, NULL);
     }
   else
     zgs->layout = elm_layout_add(parent);

   zgs->events = evas_object_rectangle_add(evas_object_evas_get(parent));
   evas_object_pointer_mode_set(zgs->events, EVAS_OBJECT_POINTER_MODE_NOGRAB);
   evas_object_smart_member_add(zgs->events, zgs->layout);
   evas_object_color_set(zgs->events, 0, 0, 0, 0);
   evas_object_repeat_events_set(zgs->events, 1);
   evas_object_show(zgs->events);
   evas_object_event_callback_add(zgs->events, EVAS_CALLBACK_MOUSE_DOWN, _site_mouse_down, zgs);

   evas_object_data_set(zgs->layout, "__z_gadget_site", zgs);

   sites = eina_list_append(sites, zgs);

   if (!move_act)
     {
        move_act = e_action_add("gadget_modify");
        e_action_predef_name_set(D_("Gadgets"), D_("Move gadget"), "gadget_modify", NULL, NULL, 0);
        move_act->func.go_mouse = _gadget_act_modify;
        move_act->func.end_mouse = _gadget_act_modify_end;
     }

   return zgs->layout;
}

Z_API Z_Gadget_Site_Gravity
z_gadget_site_gravity_get(Evas_Object *obj)
{
   Z_Gadget_Site *zgs;

   ZGS_GET(obj);
   return zgs->gravity;
}

Z_API void
z_gadget_site_gadget_add(Evas_Object *obj, const char *type)
{
   char buf[1024];
   Z_Gadget_Create_Cb cb;
   Evas_Object *g;
   Z_Gadget_Site *zgs;
   Z_Gadget_Config *zgc;
   unsigned int id = 0;

   EINA_SAFETY_ON_NULL_RETURN(gadget_types);
   ZGS_GET(obj);

   strncpy(buf, type, sizeof(buf));

   cb = eina_hash_find(gadget_types, buf);
   EINA_SAFETY_ON_NULL_RETURN(cb);

   /* if id is 0, gadget creates new config and returns id
    * otherwise, config of `id` is applied to created object
    */
   g = cb(obj, &id);
   EINA_SAFETY_ON_NULL_RETURN(g);

   zgc = E_NEW(Z_Gadget_Config, 1);
   zgc->e_obj_inherit = E_OBJECT_ALLOC(E_Object, Z_GADGET_TYPE, _gadget_object_free);
   e_object_data_set(zgc->e_obj_inherit, g);
   zgc->id = id;
   zgc->type = eina_stringshare_add(buf);
   zgc->gadget = g;
   zgc->x = -1;
   zgc->y = -1;
   zgc->site = zgs;
   evas_object_data_set(g, "__z_gadget", zgc);

   evas_object_event_callback_add(g, EVAS_CALLBACK_DEL, _gadget_del, zgc);
   zgs->gadgets = eina_list_sorted_insert(zgs->gadgets, (Eina_Compare_Cb)_site_gadgets_sort, zgc);
   _gadget_reparent(zgs, g);
   evas_object_raise(zgs->events);

   evas_object_smart_callback_call(obj, "gadget_added", g);
   evas_object_smart_callback_call(obj, "gadget_gravity", g);

   evas_object_show(g);
}

Z_API Evas_Object *
z_gadget_site_get(Evas_Object *g)
{
   Z_Gadget_Site *zgs;

   EINA_SAFETY_ON_NULL_RETURN_VAL(g, NULL);
   zgs = evas_object_data_get(g, "__z_gadget");
   EINA_SAFETY_ON_NULL_RETURN_VAL(zgs, NULL);
   return zgs->layout;
}

Z_API void
z_gadget_type_add(const char *type, Z_Gadget_Create_Cb callback)
{
   if (!gadget_types) gadget_types = eina_hash_string_superfast_new(NULL);
   eina_hash_add(gadget_types, type, callback);
}

Z_API void
z_gadget_type_del(const char *type)
{
   Eina_List *l, *ll;
   Z_Gadget_Site *zgs;
   Z_Gadget_Config *zgc;
   char buf[1024];

   strncpy(buf, type, sizeof(buf));

   if (!gadget_types) return;

   EINA_LIST_FOREACH(sites, l, zgs)
     EINA_LIST_FOREACH(zgs->gadgets, ll, zgc)
       if (eina_streq(buf, zgc->type))
         evas_object_del(zgc->gadget);
}
