#include <gtk/gtk.h>
#include <adwaita.h>
#include <iostream>
#include <format>
#include <vector>
#include <algorithm>
#include <optional>

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"


std::vector<std::string> played_file_path;

ma_engine engine;

ma_sound sound;
ma_uint64 sound_length;
float sound_length_s;
int end_min;
int end_s;
std::string end_time;
bool is_sound_init = false;

GtkWidget* song_list;
double volume = 0.1;
const std::array<std::string, 6> file_types = {".ogg", ".wav", ".mp3", ".flac", ".aiff", ".alac"}; 

gulong progress_bar_id = 0;
struct on_volume_change_data {
	GtkWidget* scale;
	GtkWidget* icon;
};
struct timestamp_labels {
	GtkWidget* start;
	GtkWidget* end;
};

static bool check_valid_format(std::string_view file_name) {
	// * goes through every file in the file dialog and checks weather the type is correct

	for (auto ext : file_types) {
		if (file_name.find(ext) != std::string_view::npos)
			return true;
	}
	return false;
}

static void append_songs_to_list(std::vector<std::string>* file_names) {
	// * Gets files from the file dialog result and adds them to a list box

	for (std::string name : *file_names) {
		for (std::string ext : file_types) {
			auto start_pos = name.find(ext);
			if (start_pos > name.size())
				continue;
			name.erase(start_pos, ext.size());
		}
		auto song_label = gtk_label_new(name.c_str());
		gtk_widget_set_vexpand(song_label, false);
		gtk_list_box_append(GTK_LIST_BOX(song_list), song_label);
	}
}

static void get_file_dialog_result( GObject* source_object, GAsyncResult* res,[[maybe_unused]] gpointer data ) {

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

static void on_open_button_click([[maybe_unused]]GtkButton* a, void* user_data) {

	GtkFileDialog* file_chooser = gtk_file_dialog_new();
	gtk_file_dialog_select_folder(file_chooser, GTK_WINDOW (user_data), NULL, get_file_dialog_result, NULL);
	g_object_unref(file_chooser);
}
static void save_sound_length() {
// * Gets the current length of the played sound and saves it in a global variable

	is_sound_init = true;
	ma_sound_get_length_in_pcm_frames(&sound, &sound_length);
	ma_sound_get_length_in_seconds(&sound, &sound_length_s);
	end_min = int(sound_length_s) / 60;
	end_s = int(sound_length_s) % 60;
	end_time = std::to_string(end_min) + (end_s < 10 ? ":0" : ":") + std::to_string(end_s);
}

static void on_play_button_click(GtkButton*, void* data) {
// * Plays the selected song and resets the current_time label to 0:00

	if (played_file_path.size() == 0)
		return;

	GtkListBoxRow* selected_row = gtk_list_box_get_selected_row(GTK_LIST_BOX(song_list));

	if (selected_row == NULL)
		return;

	std::string played_file = played_file_path[gtk_list_box_row_get_index(selected_row)];
	if (is_sound_init)
		ma_sound_uninit(&sound);
	ma_engine_set_volume(&engine, volume);

	if (ma_sound_init_from_file(&engine, played_file.c_str(),0, NULL, NULL, &sound) != MA_SUCCESS) {
		std::cerr << "CANNOT INIT SOUND";
		return;
	}

	save_sound_length();

	gtk_range_set_value(GTK_RANGE(data), 0);

	if (ma_sound_start(&sound) != MA_SUCCESS) {
		std::cerr << "CANNOT START SOUND \n";
		return;
	}

}
static void on_timestamp_change(GtkRange* range, void*) {
	// * Called on value-changed signal on the progress_bar widget
	// * Changes the sound time

	if (!is_sound_init)
		return;
	double value = gtk_range_get_value(range);
	ma_sound_seek_to_pcm_frame(&sound, value * sound_length);
}

static gboolean progress_bar_tick(GtkWidget* progress_bar, GdkFrameClock* , void* data) {
	// *	Moves the bar slider according to the time passed in the audio file

	if (!is_sound_init) {
		// * RANGE DOES NOT GET FOCUSED WHEN BEING MOVED 
		// gtk_widget_set_can_focus(progress_bar, false);
		return G_SOURCE_CONTINUE;
	}

	auto bar = GTK_RANGE(progress_bar);
	auto labels = (timestamp_labels *) data;
	double value = double (ma_sound_get_time_in_pcm_frames(&sound)) / double(sound_length);

	gtk_label_set_text(GTK_LABEL(labels->end), end_time.c_str());

	double current_ms = double (ma_sound_get_time_in_milliseconds(&sound));
	int current_s = current_ms / 1000;
	int current_min = current_s / 60;
	
	std::string current_time = std::to_string(current_min) + (current_s % 60 < 10 ? ":0" : ":") + std::to_string(current_s % 60);
	
	gtk_label_set_text(GTK_LABEL(labels->start), current_time.c_str());

	if (progress_bar_id != 0)
		g_signal_handler_disconnect(bar, progress_bar_id);
	
	gtk_label_set_text(GTK_LABEL(labels->start), gtk_label_get_text(GTK_LABEL(labels->start)));
	gtk_range_set_value(bar, value);

	progress_bar_id = g_signal_connect(progress_bar, "value-changed", G_CALLBACK(on_timestamp_change), NULL);

	return G_SOURCE_CONTINUE;
}


static void on_volume_change(GtkRange* range, void* volume_data) {
// * Changes volume icon accordingly
	auto data = (on_volume_change_data*) volume_data;
	volume = gtk_range_get_value(range) / 100;

	if (volume >= 1)
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

static GtkWidget* create_gui(GtkWidget* window) {

	GtkWidget* tool_bar = adw_header_bar_new();
	GtkWidget* play_button = gtk_button_new_with_label("Play");
	GtkWidget* open_button = gtk_button_new_with_label("Open");
	GtkWidget* progress_bar = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 1, 0.1);

	GtkWidget* progress_bar_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
	GtkWidget* control_button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
	GtkWidget* scrollable_song_box = gtk_scrolled_window_new();

	timestamp_labels* labels = new timestamp_labels{
		.start = gtk_label_new("0:00"),
		.end = gtk_label_new("0:00"),
	};

	on_volume_change_data* volume_data = new on_volume_change_data {
		.scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 100, 1),
		.icon = gtk_image_new_from_icon_name("audio-volume-medium-symbolic"),
	};

//TODO: Implement constraint layout
	// GtkLayoutManager* progress_constraint = gtk_constraint_layout_new();
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
	
	progress_bar_id = g_signal_connect(progress_bar, "value-changed", G_CALLBACK(on_timestamp_change), NULL);
	g_signal_connect(volume_data->scale, "value-changed", G_CALLBACK(on_volume_change), volume_data);
	g_signal_connect(play_button, "clicked", G_CALLBACK (on_play_button_click), progress_bar);
	g_signal_connect(open_button, "clicked", G_CALLBACK(on_open_button_click), window);

	gtk_widget_add_tick_callback(progress_bar, progress_bar_tick, labels, NULL);
	
	GtkWidget* main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrollable_song_box), song_list);

	gtk_box_append(GTK_BOX(control_button_box), play_button);
	gtk_box_append(GTK_BOX(control_button_box), open_button);
	gtk_box_append(GTK_BOX(control_button_box), volume_data->icon);
	gtk_box_append(GTK_BOX(control_button_box), volume_data->scale);

	gtk_box_append(GTK_BOX(progress_bar_box), labels->start);
	gtk_box_append(GTK_BOX(progress_bar_box), progress_bar);
	gtk_box_append(GTK_BOX(progress_bar_box), labels->end);

	gtk_box_append(GTK_BOX(main_box), tool_bar);
	gtk_box_append(GTK_BOX(main_box), scrollable_song_box);
	gtk_box_append(GTK_BOX(main_box), progress_bar_box);
	gtk_box_append(GTK_BOX(main_box), control_button_box);

	return main_box;
}

static void activate_cb (GtkApplication *app)
{
	GtkWidget* window = adw_application_window_new (app);
	
	gtk_window_set_title (GTK_WINDOW (window), "AudioPlayer");
	gtk_window_set_default_size (GTK_WINDOW (window), 1200, 700);
	gtk_widget_set_size_request(window, 300, 300);

	adw_application_window_set_content(ADW_APPLICATION_WINDOW (window), create_gui(window));

	gtk_window_present (GTK_WINDOW (window));
}

int main(int argc, char* argv[])
{
	ma_engine_config engineConfig = ma_engine_config_init();
	ma_resource_manager resource_manager;
	auto resource_manager_config = ma_resource_manager_config_init();

	ma_resource_manager_init(&resource_manager_config, &resource_manager);
	engineConfig.pResourceManager = &resource_manager;

	if (ma_engine_init(&engineConfig, &engine) != MA_SUCCESS) {
		std::cerr << "failed to init ma_engine from miniaudio library\n";
		std::abort();
	}
	
	auto app = adw_application_new("org.gitcommitcrew.audio", G_APPLICATION_DEFAULT_FLAGS);

	g_signal_connect (app, "activate", G_CALLBACK (activate_cb), NULL);

	auto result_code = g_application_run(G_APPLICATION (app), argc, argv);
	ma_engine_uninit(&engine);
	return result_code;
}