#include <errno.h>
#include <gtk/gtk.h>
#include <locale.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_BUF 128

typedef struct {
    GtkWidget *display;
    char input_buf[MAX_BUF];
    double accumulator;
    char pending_op;
    bool has_accumulator;
    bool new_entry;
    bool error_state;
} CalcState;

static void set_display_text(CalcState *st, const char *text) {
    gtk_entry_set_text(GTK_ENTRY(st->display), text);
}

static void reset_state(CalcState *st) {
    st->accumulator = 0.0;
    st->pending_op = '\0';
    st->has_accumulator = false;
    st->new_entry = true;
    st->error_state = false;
    snprintf(st->input_buf, MAX_BUF, "0");
    set_display_text(st, st->input_buf);
}

static bool parse_double_locale_safe(const char *str, double *out_val) {
    if (!str || *str == '\0' || strcmp(str, "-") == 0 || strcmp(str, ".") == 0) {
        return false;
    }

    char normalized[MAX_BUF];
    snprintf(normalized, MAX_BUF, "%s", str);
    for (size_t i = 0; normalized[i] != '\0'; i++) {
        if (normalized[i] == ',') {
            normalized[i] = '.';
        }
    }

    char *old_locale = setlocale(LC_NUMERIC, NULL);
    char saved_locale[128] = "C";
    if (old_locale) {
        snprintf(saved_locale, sizeof(saved_locale), "%s", old_locale);
    }
    setlocale(LC_NUMERIC, "C");

    errno = 0;
    char *endptr = NULL;
    double val = strtod(normalized, &endptr);

    setlocale(LC_NUMERIC, saved_locale);

    if (endptr == normalized || *endptr != '\0' || !isfinite(val)) {
        return false;
    }

    *out_val = val;
    return true;
}

static void format_double_clean(double val, char *out, size_t size) {
    if (fabs(val) < 1e-12) {
        val = 0.0;
    }

    char *old_locale = setlocale(LC_NUMERIC, NULL);
    char saved_locale[128] = "C";
    if (old_locale) {
        snprintf(saved_locale, sizeof(saved_locale), "%s", old_locale);
    }
    setlocale(LC_NUMERIC, "C");

    snprintf(out, size, "%.10g", val);

    setlocale(LC_NUMERIC, saved_locale);
}

static bool apply_binary_op(double lhs, double rhs, char op, double *result) {
    switch (op) {
        case '+':
            *result = lhs + rhs;
            return isfinite(*result);
        case '-':
            *result = lhs - rhs;
            return isfinite(*result);
        case '*':
            *result = lhs * rhs;
            return isfinite(*result);
        case '/':
            if (rhs == 0.0) {
                return false;
            }
            *result = lhs / rhs;
            return isfinite(*result);
        default:
            *result = rhs;
            return true;
    }
}

static void on_digit_clicked(GtkButton *button, gpointer user_data) {
    CalcState *st = (CalcState *)user_data;
    if (st->error_state) {
        return;
    }

    const char *label = gtk_button_get_label(button);

    if (st->new_entry) {
        snprintf(st->input_buf, MAX_BUF, "%s", label);
        st->new_entry = false;
    } else {
        if (strcmp(st->input_buf, "0") == 0) {
            snprintf(st->input_buf, MAX_BUF, "%s", label);
        } else if (strlen(st->input_buf) + 1 < MAX_BUF) {
            strncat(st->input_buf, label, MAX_BUF - strlen(st->input_buf) - 1);
        }
    }

    set_display_text(st, st->input_buf);
}

static void on_decimal_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    CalcState *st = (CalcState *)user_data;
    if (st->error_state) {
        return;
    }

    if (st->new_entry) {
        snprintf(st->input_buf, MAX_BUF, "0.");
        st->new_entry = false;
        set_display_text(st, st->input_buf);
        return;
    }

    if (strchr(st->input_buf, '.') != NULL) {
        return;
    }

    if (strlen(st->input_buf) + 1 < MAX_BUF) {
        strncat(st->input_buf, ".", MAX_BUF - strlen(st->input_buf) - 1);
        set_display_text(st, st->input_buf);
    }
}

static void on_operator_clicked(GtkButton *button, gpointer user_data) {
    CalcState *st = (CalcState *)user_data;
    if (st->error_state) {
        return;
    }

    const char *label = gtk_button_get_label(button);
    char op = label[0];

    double current_val = 0.0;
    if (!parse_double_locale_safe(st->input_buf, &current_val)) {
        current_val = 0.0;
    }

    if (!st->has_accumulator) {
        st->accumulator = current_val;
        st->has_accumulator = true;
    } else if (!st->new_entry && st->pending_op != '\0') {
        double res = 0.0;
        if (!apply_binary_op(st->accumulator, current_val, st->pending_op, &res)) {
            st->error_state = true;
            set_display_text(st, "Error: Div/0");
            return;
        }
        st->accumulator = res;
        format_double_clean(st->accumulator, st->input_buf, MAX_BUF);
        set_display_text(st, st->input_buf);
    }

    st->pending_op = op;
    st->new_entry = true;
}

static void on_equals_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    CalcState *st = (CalcState *)user_data;
    if (st->error_state || st->pending_op == '\0' || !st->has_accumulator) {
        return;
    }

    double current_val = 0.0;
    if (!parse_double_locale_safe(st->input_buf, &current_val)) {
        current_val = st->accumulator;
    }

    double res = 0.0;
    if (!apply_binary_op(st->accumulator, current_val, st->pending_op, &res)) {
        st->error_state = true;
        set_display_text(st, "Error: Div/0");
        return;
    }

    st->accumulator = res;
    format_double_clean(st->accumulator, st->input_buf, MAX_BUF);
    set_display_text(st, st->input_buf);
    st->pending_op = '\0';
    st->has_accumulator = false;
    st->new_entry = true;
}

static void on_clear_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    CalcState *st = (CalcState *)user_data;
    reset_state(st);
}

static void build_ui(GtkApplication *app, gpointer user_data) {
    CalcState *st = (CalcState *)user_data;

    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Calculadora C - GTK+ 3");
    gtk_window_set_default_size(GTK_WINDOW(window), 320, 420);
    gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(window), 12);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_row_homogeneous(GTK_GRID(grid), TRUE);
    gtk_grid_set_column_homogeneous(GTK_GRID(grid), TRUE);
    gtk_container_add(GTK_CONTAINER(window), grid);

    st->display = gtk_entry_new();
    gtk_editable_set_editable(GTK_EDITABLE(st->display), FALSE);
    gtk_widget_set_can_focus(st->display, FALSE);
    gtk_entry_set_alignment(GTK_ENTRY(st->display), 1.0);
    gtk_grid_attach(GTK_GRID(grid), st->display, 0, 0, 4, 1);

    reset_state(st);

    const char *row0[4] = {"C", "/", "*", "-"};
    for (int col = 0; col < 4; col++) {
        GtkWidget *btn = gtk_button_new_with_label(row0[col]);
        gtk_grid_attach(GTK_GRID(grid), btn, col, 1, 1, 1);
        if (strcmp(row0[col], "C") == 0) {
            g_signal_connect(btn, "clicked", G_CALLBACK(on_clear_clicked), st);
        } else {
            g_signal_connect(btn, "clicked", G_CALLBACK(on_operator_clicked), st);
        }
    }

    const char *digits_r1[3] = {"7", "8", "9"};
    for (int col = 0; col < 3; col++) {
        GtkWidget *btn = gtk_button_new_with_label(digits_r1[col]);
        gtk_grid_attach(GTK_GRID(grid), btn, col, 2, 1, 1);
        g_signal_connect(btn, "clicked", G_CALLBACK(on_digit_clicked), st);
    }
    GtkWidget *btn_plus = gtk_button_new_with_label("+");
    gtk_grid_attach(GTK_GRID(grid), btn_plus, 3, 2, 1, 2);
    g_signal_connect(btn_plus, "clicked", G_CALLBACK(on_operator_clicked), st);

    const char *digits_r2[3] = {"4", "5", "6"};
    for (int col = 0; col < 3; col++) {
        GtkWidget *btn = gtk_button_new_with_label(digits_r2[col]);
        gtk_grid_attach(GTK_GRID(grid), btn, col, 3, 1, 1);
        g_signal_connect(btn, "clicked", G_CALLBACK(on_digit_clicked), st);
    }

    const char *digits_r3[3] = {"1", "2", "3"};
    for (int col = 0; col < 3; col++) {
        GtkWidget *btn = gtk_button_new_with_label(digits_r3[col]);
        gtk_grid_attach(GTK_GRID(grid), btn, col, 4, 1, 1);
        g_signal_connect(btn, "clicked", G_CALLBACK(on_digit_clicked), st);
    }
    GtkWidget *btn_eq = gtk_button_new_with_label("=");
    gtk_grid_attach(GTK_GRID(grid), btn_eq, 3, 4, 1, 2);
    g_signal_connect(btn_eq, "clicked", G_CALLBACK(on_equals_clicked), st);

    GtkWidget *btn_zero = gtk_button_new_with_label("0");
    gtk_grid_attach(GTK_GRID(grid), btn_zero, 0, 5, 2, 1);
    g_signal_connect(btn_zero, "clicked", G_CALLBACK(on_digit_clicked), st);

    GtkWidget *btn_dot = gtk_button_new_with_label(".");
    gtk_grid_attach(GTK_GRID(grid), btn_dot, 2, 5, 1, 1);
    g_signal_connect(btn_dot, "clicked", G_CALLBACK(on_decimal_clicked), st);

    gtk_widget_show_all(window);
}

int main(int argc, char **argv) {
    CalcState state;
    memset(&state, 0, sizeof(state));

    GtkApplication *app = gtk_application_new("cl.cftsa.calculadora", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(build_ui), &state);

    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}