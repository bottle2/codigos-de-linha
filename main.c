/* nuklear - 1.32.0 - public domain */

#include <stddef.h>
#include <string.h>
#include <math.h>

#include <SDL2/SDL.h>

#define NK_PRIVATE
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_ZERO_COMMAND_MEMORY

#define NK_ASSERT    SDL_assert
#define NK_MEMSET    memset
#define NK_SQRT      sqrt
#define NK_SIN       sin
#define NK_COS       cos
#define NK_VSNPRINTF vsnprintf

#define NK_IMPLEMENTATION
#define NK_SDL_RENDERER_IMPLEMENTATION
#include "nuklear.h"
#include "nuklear_sdl_renderer.h"

#define TITLE "Visualização de códigos de linha"

#define MAX_DIGITS 512

#define AS_FIRST(     A,       ...) A
#define AS_SECOND(    A, B,    ...) B
#define AS_SECOND_END(A, B        ) B
#define AS_THIRD(     A, B, C, ...) C
#define AS_THIRD_END( A, B, C     ) C
#define AS_FOURTH_END(A, B, C, D  ) D

#define SIGNAL_XS(X)                      \
X(SIGNAL_LOW        , SIGNAL_HIGH       ), \
X(SIGNAL_HIGH       , SIGNAL_LOW        ), \
X(SIGNAL_ZERO       , SIGNAL_ZERO       ), \
X(SIGNAL_LOW_TO_HIGH, SIGNAL_HIGH_TO_LOW), \
X(SIGNAL_HIGH_TO_LOW, SIGNAL_LOW_TO_HIGH)
//Enumeration         Opposite

enum                     signal              { SIGNAL_XS(AS_FIRST     ) };
static enum signal const signal_opposite[] = { SIGNAL_XS(AS_SECOND_END) };

#define CODE_XS(X)                                                                                                           \
X(CODE_NRZ_L                  , "NRZ-L: RS-232"                                  , SIGNAL_ZERO        , SIGNAL_ZERO         ), \
X(CODE_NRZ_I                  , "NRZ-I: USB"                                     , SIGNAL_LOW         , SIGNAL_ZERO         ), \
X(CODE_AMI                    , "AMI: tronco digital T1 (ISDN primário 1544Mbps)", SIGNAL_ZERO        , SIGNAL_LOW          ), \
X(CODE_PSEUDOTERNARY          , "Pseudoternário: interface ISDN básica (192kbps)", SIGNAL_ZERO        , SIGNAL_LOW          ), \
X(CODE_MANCHESTER             , "Manchester: IEEE 802.3"                         , SIGNAL_ZERO        , SIGNAL_ZERO         ), \
X(CODE_MANCHESTER_DIFFERENTIAL, "Manchester Diferencial: IEEE 802.5"             , SIGNAL_LOW_TO_HIGH , SIGNAL_ZERO         )
//Enumeration                   Label                                              Initial last signal  Initial last bipolar

enum                      code                          { CODE_XS(AS_FIRST     ), N_CODE };
static char const        *code_labels              [] = { CODE_XS(AS_SECOND    )         };
static enum signal const  code_initial_last_signal [] = { CODE_XS(AS_THIRD     )         };
static enum signal const  code_initial_last_bipolar[] = { CODE_XS(AS_FOURTH_END)         };

static SDL_Window          * main_create_window_or_die(int width, int height);
static SDL_Renderer        * main_create_renderer_or_die(SDL_Window *window);
static float                 main_scale_font(SDL_Window *window, SDL_Renderer *renderer);
static struct nk_user_font * main_load_font(float font_scale);

static void main_event(struct nk_context *ctx, SDL_Event *event, int *width, int *height);
static void main_make_gui(struct nk_context *ctx, int width, int height);
static void main_view(struct nk_context *ctx, char const *data, int n_data, enum code code);

static enum signal main_code(enum code code, char bit, enum signal last_signal, enum signal last_bipolar);

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    SDL_SetHint(SDL_HINT_VIDEO_HIGHDPI_DISABLED, "0");
    SDL_Init(SDL_INIT_VIDEO);

    int                width      = 640;
    int                height     = 480;
    SDL_Window        *window     = main_create_window_or_die(width, height);
    SDL_Renderer      *renderer   = main_create_renderer_or_die(window);
    float              font_scale = main_scale_font(window, renderer);
    struct nk_context *ctx        = nk_sdl_init(window, renderer, NULL);

    nk_style_set_font(ctx, main_load_font(font_scale));

    for (SDL_Event event; event.type != SDL_QUIT; SDL_WaitEvent(&event))
    {
        main_event(ctx, &event, &width, &height);
        main_make_gui(ctx, width, height);
        SDL_RenderPresent(renderer);
    }

    nk_sdl_shutdown();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}

static SDL_Window * main_create_window_or_die(int width, int height)
{
    SDL_Window *window = SDL_CreateWindow(
        TITLE,
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        width,
        height,
        SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE
    );

    if (NULL == window)
    {
        SDL_Log("Couldn't create window: %s", SDL_GetError());
        exit(EXIT_FAILURE);
    }

    return window;
}

static SDL_Renderer * main_create_renderer_or_die(SDL_Window *window)
{
    SDL_Renderer *renderer = SDL_CreateRenderer(
        window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    if (NULL == renderer)
    {
        SDL_Log("Couldn't create renderer: %s", SDL_GetError());
        exit(EXIT_FAILURE);
    };
    
    return renderer;
}

static float main_scale_font(SDL_Window *window, SDL_Renderer *renderer)
{
    int render_w = -1;
    int render_h = -1;
    int window_w = -1;
    int window_h = -1;

    SDL_GetRendererOutputSize(renderer, &render_w, &render_h);
    SDL_GetWindowSize(window, &window_w, &window_h);

    float scale_x = (float)(render_w) / (float)(window_w);
    float scale_y = (float)(render_h) / (float)(window_h);

    SDL_RenderSetScale(renderer, scale_x, scale_y);

    return scale_y;
}

static struct nk_user_font * main_load_font(float font_scale)
{
    struct nk_font_atlas *atlas;

    /* set up the font atlas and add desired font; note that font sizes are
     * multiplied by font_scale to produce better results at higher DPIs */
    nk_sdl_font_stash_begin(&atlas);

    struct nk_font *font = nk_font_atlas_add_from_file(
        atlas,
        "Roboto-Regular.ttf",
        16 * font_scale,
        NULL
    );

    nk_sdl_font_stash_end();

    /* this hack makes the font appear to be scaled down to the desired
     * size and is only necessary when font_scale > 1 */
    font->handle.height /= font_scale;
    /*nk_style_load_all_cursors(ctx, atlas->cursors);*/

    return &font->handle;
}

static void main_event(struct nk_context *ctx, SDL_Event *event, int *width, int *height)
{
    if (SDL_WINDOWEVENT                 == event->type
        && SDL_WINDOWEVENT_SIZE_CHANGED == event->window.event)
    {
        *width  = event->window.data1;
        *height = event->window.data2;
    }

    nk_input_begin(ctx);
    nk_sdl_handle_event(event);
    nk_input_end(ctx);
}

static void main_make_gui(struct nk_context *ctx, int width, int height)
{
    if (nk_begin(ctx, TITLE, nk_rect(0, 0, width, height),
                NK_WINDOW_BACKGROUND | NK_WINDOW_NO_SCROLLBAR))
    {
        static char      data[MAX_DIGITS] = "01001100011";
        static int       n_data           = 11;
        static enum code code             = CODE_NRZ_L;

        SDL_assert(strlen(data) == (size_t)n_data);

        nk_layout_row_dynamic(ctx, 0, 1);
	nk_label(ctx, TITLE, NK_TEXT_LEFT);

        for (enum code code_i = 0; code_i < N_CODE; code_i++)
        {
            code = nk_option_label(ctx, code_labels[code_i], code == code_i) ? code_i : code;
        }

        nk_label(ctx, "Sequência de bits:", NK_TEXT_LEFT);
        nk_edit_string(ctx, NK_EDIT_SIMPLE, data, &n_data, MAX_DIGITS, nk_filter_binary);

        main_view(ctx, data, n_data, code);
    }
    nk_end(ctx);

    nk_sdl_render(NK_ANTI_ALIASING_ON);
}

static void main_view(struct nk_context *ctx, char const *data, int n_data, enum code code)
{
    nk_layout_row_dynamic(ctx, 50, 1);

    struct nk_command_buffer *painter = nk_window_get_canvas(ctx);
    struct nk_rect            bounds  = nk_layout_widget_bounds(ctx);

    float x_min       = bounds.x;
    float x_max       = bounds.x + bounds.w;
    float x_part      =  bounds.w / (float)n_data;
    float x_part_half = (bounds.w / (float)n_data) / 2.0f;
    float y_min       = bounds.y;
    float y_max       = bounds.y + bounds.h;
    float y_mid       = bounds.y + bounds.h / 2.0f;

    enum signal last_signal  = code_initial_last_signal [code];
    enum signal last_bipolar = code_initial_last_bipolar[code];

    nk_stroke_line(painter, x_min, y_mid, x_max, y_mid, 1, nk_red);
    
    float xys[MAX_DIGITS * 4 * 2] = {0};
    int n_point = 0;

    for (int data_i = 0; data_i < n_data; data_i++)
    {
        float x_this = x_min + x_part * data_i;
        float x_this_halfway = x_this + x_part_half;
        float x_this_next    = x_this + x_part;

        enum signal signal = main_code(code, data[data_i], last_signal, last_bipolar);
        float       xs[4]  = {0};
        float       ys[4]  = {0};
	int         n_xy   = 0;

        nk_stroke_line(painter, x_this, y_min, x_this, y_max, 1, nk_red);

        switch (signal)
        {
            case SIGNAL_LOW:
                xs[0]         = x_this;
                xs[1]         = x_this_next;
                ys[0] = ys[1] = y_max;
		n_xy = 2;
            break;

            case SIGNAL_HIGH:
                xs[0]         = x_this;
                xs[1]         = x_this_next;
                ys[0] = ys[1] = y_min;
		n_xy = 2;
            break;

            case SIGNAL_ZERO:
                xs[0]         = x_this;
                xs[1]         = x_this_next;
                ys[0] = ys[1] = y_mid;
		n_xy = 2;
            break;

            case SIGNAL_LOW_TO_HIGH:
                xs[0] = x_this;
                ys[0] = y_max;
                xs[1] = x_this_halfway;
                ys[1] = y_max;
                xs[2] = x_this_halfway;
                ys[2] = y_min;
                xs[3] = x_this_next;
                ys[3] = y_min;
		n_xy = 4;
            break;

            case SIGNAL_HIGH_TO_LOW:
                xs[0] = x_this;
                ys[0] = y_min;
                xs[1] = x_this_halfway;
                ys[1] = y_min;
                xs[2] = x_this_halfway;
                ys[2] = y_max;
                xs[3] = x_this_next;
                ys[3] = y_max;
		n_xy = 4;
            break;

            default:
                SDL_assert(!"Impossible signal!");
            break;
        }


        for (int point_i = 0; point_i < n_xy; point_i++)
        {
            if (n_point > 0)
            {
                if (!(xys[(n_point - 1) * 2] == xs[point_i] && xys[(n_point - 1) * 2 + 1] == ys[point_i]))
                {
                    xys[n_point * 2]     = xs[point_i];
                    xys[n_point * 2 + 1] = ys[point_i];
                    n_point++;
                }
            }
            else
            {
                xys[n_point * 2]     = xs[point_i];
                xys[n_point * 2 + 1] = ys[point_i];
                n_point++;
            }
        }

        last_signal  = signal;
        last_bipolar = signal != SIGNAL_ZERO ? signal : last_bipolar; 
    }

    nk_stroke_line(painter, x_max, y_min, x_max, y_max, 1, nk_red);

    nk_stroke_polyline(painter, xys, n_point, 1, nk_yellow);
}

static enum signal main_code(enum code code, char bit, enum signal last_signal, enum signal last_bipolar)
{
    enum signal signal = SIGNAL_ZERO;

    switch (code)
    {
        case CODE_NRZ_L:                   signal = '0' == bit ? SIGNAL_HIGH        : SIGNAL_LOW                   ; break;
        case CODE_NRZ_I:                   signal = '0' == bit ? last_signal        : signal_opposite[last_signal] ; break;
        case CODE_AMI:                     signal = '0' == bit ? SIGNAL_ZERO        : signal_opposite[last_bipolar]; break;
        case CODE_PSEUDOTERNARY:           signal = '1' == bit ? SIGNAL_ZERO        : signal_opposite[last_bipolar]; break;
        case CODE_MANCHESTER:              signal = '0' == bit ? SIGNAL_HIGH_TO_LOW : SIGNAL_LOW_TO_HIGH           ; break;
        case CODE_MANCHESTER_DIFFERENTIAL: signal = '0' == bit ? last_signal        : signal_opposite[last_signal] ; break;

        default:
            SDL_assert(!"No such encoding.");
        break;
    }

    return signal;
}
