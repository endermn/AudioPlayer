#include <gtk/gtk.h>
#include <adwaita.h>
#include <iostream>
#include <aubio/aubio.h>
#include <format>
#include <vector>
#include <algorithm>
#include <optional>

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

std::vector<std::string> played_file_path;

ma_engine engine;
ma_engine default_engine;
ma_sound sound;

GtkWidget* song_list;
double volume = 1;

struct on_volume_change_data {
	GtkWidget* scale;
	GtkWidget* icon;
};


static bool check_valid_format(std::string_view file_name) {
	
	const std::array<std::string, 6> file_types = {".ogg", ".wav", ".mp3", ".flac", ".aiff", ".alac"}; 

	for (auto ext : file_types) {
		if (file_name.find(ext) != std::string_view::npos)
			return true;
	}
	return false;
}

static void append_songs_to_list(std::vector<std::string>* file_names) {
	
	for (std::string name : *file_names) {
		auto song_label = gtk_label_new(name.c_str());
		gtk_widget_set_vexpand(song_label, false);
		gtk_list_box_append(GTK_LIST_BOX(song_list), song_label);
	}
}

static void get_file_dialog_result( GObject* source_object, GAsyncResult* res, gpointer data) {

	GFile* file = gtk_file_dialog_select_folder_finish(GTK_FILE_DIALOG(source_object), res, NULL);
	if (file == NULL)
		return;

	std::vector<std::string> file_names;
	
	GFileEnumerator* enumerator = g_file_enumerate_children(file, "", G_FILE_QUERY_INFO_NONE, NULL, NULL);

	while(true) {
		GFileInfo* current_file = g_file_enumerator_next_file(enumerator, NULL, NULL);
		if (current_file == NULL)
			break;

		std::string file_name = g_file_info_get_name(current_file);
		if (!check_valid_format(file_name))
			continue;

		if (std::ranges::contains(file_names, file_name)) 
			continue;

		played_file_path.push_back(std::format("{}/{}", g_file_get_path(file), file_name));
		file_names.push_back(file_name);
		g_object_unref(current_file);
	}

	append_songs_to_list(&file_names);
	g_file_enumerator_close(enumerator, NULL, NULL);
	g_object_unref(file);
	g_object_unref(enumerator);
}

static void on_open_button_click(GtkButton* a, void* user_data) {

	GtkFileDialog* file_chooser = gtk_file_dialog_new();
	gtk_file_dialog_select_folder(file_chooser, GTK_WINDOW (user_data), NULL, get_file_dialog_result, NULL);
	g_object_unref(file_chooser);
}

static void on_play_button_click() {
	
	if (played_file_path.size() == 0)
		return;

	GtkListBoxRow* selected_row = gtk_list_box_get_selected_row(GTK_LIST_BOX(song_list));

	if (selected_row == NULL)
		return;

	std::string played_file = played_file_path[gtk_list_box_row_get_index(selected_row)];

	engine = default_engine;
	ma_engine_set_volume(&engine, volume);

	if (ma_engine_play_sound(&engine, played_file.c_str(), NULL) != MA_SUCCESS)
		std::cerr << "CANNOT PLAY SOUND";

}

static void on_timestamp_change() {

}

static void on_volume_change(GtkRange* range, void* volume_data) {

	auto data = (on_volume_change_data*) volume_data;
	volume = gtk_range_get_value(range) / 100;

	if (volume > 1)
		gtk_image_set_from_icon_name(GTK_IMAGE(data->icon), "audio-volume-overamplified-symbolic");
	else if (volume > 0.7)
		gtk_image_set_from_icon_name(GTK_IMAGE(data->icon), "audio-volume-high-symbolic");
	else if (volume > 0.4)
		gtk_image_set_from_icon_name(GTK_IMAGE(data->icon), "audio-volume-medium-symbolic");
	else if (volume > 0)
		gtk_image_set_from_icon_name(GTK_IMAGE(data->icon), "audio-volume-low-symbolic");
	else
		gtk_image_set_from_icon_name(GTK_IMAGE(data->icon), "audio-volume-muted-symbolic");

	ma_engine_set_volume(&engine, volume);
}

static void set_control_box(GtkWidget* control_button_box) {
	gtk_widget_set_can_focus(control_button_box, false);
	gtk_widget_set_halign(control_button_box, GTK_ALIGN_CENTER);
	gtk_widget_set_margin_top(control_button_box, 12);
	gtk_widget_set_margin_bottom(control_button_box, 10);
}

static void activate_cb (GtkApplication *app)
{
	GtkWidget* window = adw_application_window_new (app);

	GtkWidget* tool_bar = adw_header_bar_new();
	GtkWidget* play_button = gtk_button_new_with_label("Play");
	GtkWidget* open_button = gtk_button_new_with_label("Open");
	GtkWidget* progress_bar_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
	GtkWidget* control_button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);

	GtkWidget* scrollable_song_box = gtk_scrolled_window_new();

	GtkWidget* progress_bar = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
	
	on_volume_change_data* volume_data = new on_volume_change_data {
		.scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 200, 1),
		.icon = gtk_image_new_from_icon_name("audio-volume-medium-symbolic"),
	};


	GtkLayoutManager* progress_constraint = gtk_constraint_layout_new();
	// GtkConstraint* 

	// gtk_constraint_layout_add_constraint();

	song_list = gtk_list_box_new();

	gtk_range_set_value(GTK_RANGE(volume_data->scale), volume * 100);

	gtk_widget_set_size_request(progress_bar, 600, 10);
	gtk_widget_set_size_request(volume_data->scale, 200, 20);

	

	set_control_box(control_button_box);

	gtk_widget_set_halign(progress_bar_box, GTK_ALIGN_CENTER);
	gtk_widget_set_halign(volume_data->icon, GTK_ALIGN_END);
	gtk_widget_set_vexpand(song_list, true);

	g_signal_connect(progress_bar, "value-changed", G_CALLBACK(on_timestamp_change), NULL);
	g_signal_connect(volume_data->scale, "value-changed", G_CALLBACK(on_volume_change), volume_data);
	g_signal_connect(play_button, "clicked", G_CALLBACK (on_play_button_click), NULL);
	g_signal_connect(open_button, "clicked", G_CALLBACK(on_open_button_click), window);
	
	GtkWidget* main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrollable_song_box), song_list);

	gtk_box_append(GTK_BOX(control_button_box), play_button);
	gtk_box_append(GTK_BOX(control_button_box), open_button);
	gtk_box_append(GTK_BOX(progress_bar_box), progress_bar);
	gtk_box_append(GTK_BOX(control_button_box), volume_data->icon);
	gtk_box_append(GTK_BOX(control_button_box), volume_data->scale);

	gtk_box_append(GTK_BOX(main_box), tool_bar);
	gtk_box_append(GTK_BOX(main_box), scrollable_song_box);
	gtk_box_append(GTK_BOX(main_box), progress_bar_box);
	gtk_box_append(GTK_BOX(main_box), control_button_box);


	gtk_window_set_title (GTK_WINDOW (window), "AudioPlayer");
	gtk_window_set_default_size (GTK_WINDOW (window), 1200, 700);
	gtk_widget_set_size_request(window, 300, 300);

	adw_application_window_set_content(ADW_APPLICATION_WINDOW (window), main_box);

	gtk_window_present (GTK_WINDOW (window));
}

int main(int argc, char* argv[])
{
	ma_engine_config engineConfig = ma_engine_config_init();
	ma_resource_manager resource_manager;
	auto resource_manager_config = ma_resource_manager_config_init();

	ma_resource_manager_init(&resource_manager_config, &resource_manager);
	engineConfig.pResourceManager = &resource_manager;

	ma_result result = ma_engine_init(&engineConfig, &engine);
	
	default_engine = engine;

	if (result != MA_SUCCESS)
		return result;
	
	auto app = adw_application_new("org.viktor.attempt", G_APPLICATION_DEFAULT_FLAGS);

	g_signal_connect (app, "activate", G_CALLBACK (activate_cb), NULL);



	return g_application_run(G_APPLICATION (app), argc, argv);
}