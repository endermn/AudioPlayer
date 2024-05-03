#include <gtk/gtk.h>
#include <adwaita.h>

#include <taglib/taglib.h>
#include <taglib/fileref.h>
#include <taglib/tag.h>

#include <format>
#include <vector>
#include <algorithm>
#include <optional>

#include "Logger.hpp"

#define MINIAUDIO_IMPLEMENTATION
#include "include/miniaudio.h"


std::vector<std::string> played_file_path;

ma_engine engine;

ma_sound sound;
ma_uint64 sound_length;

float sound_length_s = 0;
int end_min = 0;
int end_s = 0;
std::string end_time = "";

bool is_sound_init = false;
bool is_sound_paused = false;
bool volume_changed = false;

GtkWidget* song_list;
double volume = 0.1;

GtkListBoxRow* selected_row = NULL;

const std::array<std::string, 6> file_types = {".wav", ".mp3", ".flac"}; 

long bar_id = 0;
long volume_bar_id = 0;
long double_click_id = 0;

struct on_volume_change_data {
	GtkWidget* scale;
	GtkWidget* icon;
};
struct timestamp_labels {
	GtkWidget* start;
	GtkWidget* end;
};
struct song_controller {
	GtkWidget* play_button;
	GtkWidget* open_button;
	GtkWidget* progress_bar;
	on_volume_change_data* volume_data;
};

struct song_data {
	std::string title;
	std::string author;
	std::string album;
	std::string genre;
	unsigned int year;
};

song_data played_song{"", "", "", "", 0};


static bool check_valid_format(std::string_view file_name) {
	// * goes through every file in the file dialog and checks weather the type is correct

	for (auto ext : file_types) {
		std::size_t ext_index = file_name.find(ext);
		if (ext_index == 0 || ext_index >= file_name.size())
			continue;
		// log(ext_index, INFO);

		if (ext_index == (file_name.size() - ext.size()))
			return true;
		// if (file_name.find(ext) != std::string_view::npos)
		// 	return true;
	}
	return false;
}

static bool set_song_metadata(std::string file) {
	TagLib::FileRef file_ref(file.c_str());
	auto tag = file_ref.tag();

	if (file_ref.isNull())
		return false;

	if (tag == nullptr)
		return false;

	played_song.title = tag->title().to8Bit(true);
	played_song.author = tag->artist().to8Bit(true);
	played_song.album = tag->album().to8Bit(true);
	played_song.genre = tag->genre().to8Bit(true);

	// played_song.year = tag->year();

	// std::cout << played_song.year << '\n';

	return true;
}

static void append_songs_to_list(std::vector<std::string>* file_names) {
	// * Gets files from the file dialog result and adds them to a list box
	std::vector<std::string> names = *file_names;

	for (size_t i = 0; i < names.size(); i++) {
		for (std::string ext : file_types) {
			auto start_pos = names[i].find(ext);
			if (start_pos > names[i].size())
				continue;
			names[i].erase(start_pos, ext.size());
		}


		GtkWidget* song_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
		auto title = gtk_label_new(names[i].c_str());
		gtk_box_append(GTK_BOX(song_box), title);

		gtk_widget_set_vexpand(song_box, false);
		gtk_widget_set_halign(song_box, GTK_ALIGN_START);

		if (!set_song_metadata(played_file_path[i])) {
			gtk_list_box_append(GTK_LIST_BOX(song_list), song_box);
			continue;
		}

		auto artist = gtk_label_new(("- " + played_song.author).c_str());
		auto album = gtk_label_new(("- " + played_song.album).c_str());
		auto genre = gtk_label_new(("- " + played_song.genre).c_str());

		// int year_int =  played_song.year;
		// auto year = gtk_label_new("" + year_int);

		gtk_box_append(GTK_BOX(song_box), artist);
		gtk_box_append(GTK_BOX(song_box), album);
		gtk_box_append(GTK_BOX(song_box), genre);
		// gtk_box_append(GTK_BOX(song_box), year);
		


		gtk_list_box_append(GTK_LIST_BOX(song_list), song_box);
		
	}
}

static void loop_folder(std::vector<std::string>& file_names, GFile* file) {

	GFileEnumerator* enumerator = g_file_enumerate_children(file, "", G_FILE_QUERY_INFO_NONE, NULL, NULL);
	
	if (enumerator == NULL)
		return;

	while(true) {
		GFileInfo* current_file = g_file_enumerator_next_file(enumerator, NULL, NULL);
		if (current_file == NULL)
			break;

		const char* file_name_cstr = g_file_info_get_name(current_file);
        if (file_name_cstr == NULL) {
            g_object_unref(current_file);
            continue;
        }

        std::string file_name(file_name_cstr);

		if (g_file_info_get_file_type(current_file) == G_FILE_TYPE_DIRECTORY) {
			GFile* subfolder = g_file_get_child(file, file_name.c_str());
			loop_folder(file_names, subfolder);
			g_object_unref(subfolder);
			g_object_unref(current_file);
			continue;
		}

		// log(g_file_info_get_display_name(current_file), INFO);
		// log(file_name, INFO);

		// log(file_name, INFO);
		if (!check_valid_format(file_name))
			continue;

		if (std::ranges::contains(file_names, file_name)) 
			continue;

		played_file_path.push_back(std::format("{}/{}", g_file_get_path(file), file_name));
		file_names.push_back(file_name);
		g_object_unref(current_file);
	}

	g_file_enumerator_close(enumerator, NULL, NULL);
	g_object_unref(enumerator);
}

static void get_file_dialog_result( GObject* source_object, GAsyncResult* res, void*) {

	GFile* file = gtk_file_dialog_select_folder_finish(GTK_FILE_DIALOG(source_object), res, NULL);
	if (file == NULL)
		return;

	std::vector<std::string> file_names;
	
	loop_folder(file_names, file);

	append_songs_to_list(&file_names);

	g_object_unref(file);
}

static void on_open_button_click([[maybe_unused]]GtkButton* a, void* user_data) {
// * Opens file dialog to choose a starting dir
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

static void play_sound(std::string played_file) {
	ma_sound_uninit(&sound);

	if (ma_sound_init_from_file(&engine, played_file.c_str(), 0, NULL, NULL, &sound) != MA_SUCCESS) {
		log("CANNOT INIT SOUND", ERROR);
		log(played_file, INFO);
		return;
	}

	save_sound_length();

	if (ma_sound_start(&sound) != MA_SUCCESS) {
		log("CANNOT START SOUND", ERROR);
		log(played_file, INFO);
		return;
	}
}


static void start_next_sound() {

	int selected_row_index = gtk_list_box_row_get_index(selected_row);
	int new_row_index = selected_row_index + 1;
	int total_songs = played_file_path.size();

	if (new_row_index >= total_songs) {
		new_row_index = 0;
	}

	play_sound(played_file_path[new_row_index]);

	selected_row = gtk_list_box_get_row_at_index(GTK_LIST_BOX(song_list), new_row_index);
}

static void select_sound_from_list(GtkButton* button, void* progress_bar) {
// * Plays the selected song and resets the current_time label to 0:00

	if (played_file_path.size() == 0)
		return;
	is_sound_paused = false;

	selected_row = gtk_list_box_get_selected_row(GTK_LIST_BOX(song_list));
	if (selected_row == NULL)
		return;
	
	ma_engine_set_volume(&engine, volume);

	play_sound(played_file_path[gtk_list_box_row_get_index(selected_row)]);

	gtk_range_set_value(GTK_RANGE(progress_bar), 0);

	gtk_button_set_label(button, "Pause");

}

/**
 * @brief Resumes playback of the audio.
 * 
 * 
 * @param button The button widget used for controlling playback.
 * 
 * @bug There's an issue where if you change the progress bar value while paused
 * and you proceed to play it after, the progress bar will jump back for a split second
 * after which it will proceed like normal
 */
static void sound_continue(GtkButton* button) {
	ma_sound_start(&sound);
	is_sound_paused = false;
	gtk_button_set_label(button, "Pause");
}

/**
 * @brief Pauses playback of the audio.
 * 
 * This function pauses playback of the audio by stopping the sound at the current position.
 * It sets the `is_sound_paused` flag to true to indicate that the sound is paused,
 * and updates the label of the provided button widget to "Play" to reflect the current state.
 * 
 * @param button The button widget used for controlling playback.
 */
static void sound_pause(GtkButton* button) {
	ma_sound_stop(&sound);
	is_sound_paused = true;
	gtk_button_set_label(button, "Play");
}


/**
 * @brief Toggles between pausing and resuming playback of the audio.
 * 
 * @param button The button widget used for controlling playback.
 */
static void toggle_playback_state(GtkButton* button, void* ) {
	if (!is_sound_init)
		return;
	is_sound_paused ? sound_continue(button) : sound_pause(button);
}


/** @brief
* Called on value-changed signal on the progress_bar widget
* Changes the sound time
*/
static void on_timestamp_change(GtkRange* progress_bar, void*) {
	if (!is_sound_init)
		return;
	double value = gtk_range_get_value(progress_bar);
	ma_sound_seek_to_pcm_frame(&sound, value * sound_length);
}


static gboolean progress_bar_tick(GtkWidget* progress_bar, GdkFrameClock* , void* data) {
	// * Moves the bar slider according to the time passed in the audio file

	if (!gtk_widget_is_sensitive(progress_bar) && is_sound_init)
		gtk_widget_set_sensitive(progress_bar, TRUE);
	
	if (!is_sound_init) {
		gtk_widget_set_sensitive(progress_bar, FALSE);
		return G_SOURCE_CONTINUE;
	}

	if (is_sound_paused)
		return G_SOURCE_CONTINUE;

	if (ma_sound_at_end(&sound)) {
		start_next_sound();
		gtk_range_set_value(GTK_RANGE(progress_bar), 0);
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

	if (bar_id != 0)
		g_signal_handler_disconnect(bar, bar_id);
	
	gtk_label_set_text(GTK_LABEL(labels->start), gtk_label_get_text(GTK_LABEL(labels->start)));
	gtk_range_set_value(bar, value);

	bar_id = g_signal_connect(progress_bar, "value-changed", G_CALLBACK(on_timestamp_change), NULL);

	return G_SOURCE_CONTINUE;
}

static void change_volume_icon(on_volume_change_data* data) {
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
}

static void on_volume_change(GtkRange* range, void* volume_data) {
	auto data = (on_volume_change_data*) volume_data;
	volume = gtk_range_get_value(range) / 100;
	change_volume_icon(data);
	ma_engine_set_volume(&engine, volume);
}

static gboolean on_key_pressed(GtkEventControllerKey* , int keyval, int, GdkModifierType, void* data) {
	auto song_data = (song_controller*) data;

	if (!song_data) {
		log("song_data pointer is null", ERROR);
		return GDK_EVENT_STOP;
	}
	auto bar = song_data->progress_bar;
	double range_value = gtk_range_get_value(GTK_RANGE(bar));

	switch (keyval) {
		case GDK_KEY_space:
			toggle_playback_state(GTK_BUTTON(song_data->play_button), NULL);
			break;
		case GDK_KEY_Left:
			if (!gtk_widget_is_sensitive(bar))
				return GDK_EVENT_STOP;
			range_value -= 0.1;
			if (range_value < 0) range_value = 0;
			break;
		case GDK_KEY_Right:
			if (!gtk_widget_is_sensitive(bar))
				return GDK_EVENT_STOP;
			range_value += 0.1;
			if (range_value > 1) range_value = 1;
			break;
		case GDK_KEY_Up:
			volume += 0.1;
			if (volume > 1.0) volume = 1.0;
			break;
		case GDK_KEY_Down:
			volume -= 0.1;
			if (volume < 0.0) volume = 0.0;
			break;
		default:
			break;
	}
	if (!song_data->volume_data) {
		log("volume_data pointer is null", ERROR);
		return GDK_EVENT_STOP;
	}
	gtk_range_set_value(GTK_RANGE(bar), range_value);
	gtk_range_set_value(GTK_RANGE(song_data->volume_data->scale), volume * 100);
	change_volume_icon(song_data->volume_data);
	ma_engine_set_volume(&engine, volume);

	return GDK_EVENT_STOP;
}



static void set_control_box(GtkWidget* control_button_box) {
/** @brief used to change the controlbox 
*/
	gtk_widget_set_can_focus(control_button_box, false);
	gtk_widget_set_halign(control_button_box, GTK_ALIGN_CENTER);
	gtk_widget_set_margin_top(control_button_box, 12);
	gtk_widget_set_margin_bottom(control_button_box, 10);
}

static void select_song(GtkListBox* box, GtkListBoxRow* , void* data) {
	song_controller* control = reinterpret_cast<song_controller*>(data);
	select_sound_from_list(GTK_BUTTON(control->play_button), control->progress_bar);
	gtk_list_box_unselect_all(box);
}

/** @brief main function for the gui
 * @param window is used for the gui
*/
static GtkWidget* create_gui(GtkWidget* window) {

	GtkWidget* tool_bar = adw_header_bar_new();

	timestamp_labels* labels = new timestamp_labels{
		.start = gtk_label_new("0:00"),
		.end = gtk_label_new("0:00"),
	};

	on_volume_change_data* volume_data = new on_volume_change_data {
		.scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 100, 1),
		.icon = gtk_image_new_from_icon_name("audio-volume-medium-symbolic"),
	};

	song_controller* song_control = new song_controller{
		.play_button = gtk_button_new_with_label("Play"),
		.open_button = gtk_button_new_with_label("Open"),
		.progress_bar = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 1, 0.1),
		.volume_data = volume_data,
	};

	gtk_widget_set_sensitive(song_control->progress_bar, FALSE);

	GtkWidget* progress_bar_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
	GtkWidget* control_button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
	GtkWidget* scrollable_song_box = gtk_scrolled_window_new();



//TODO: Implement constraint layout
	// GtkLayoutManager* progress_constraint = gtk_constraint_layout_new();
	// GtkConstraint* 

	// gtk_constraint_layout_add_constraint();

	song_list = gtk_list_box_new();

	GtkGesture* controller = gtk_gesture_click_new();
	gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(controller), 0);

	GtkEventController* event_controller = gtk_event_controller_key_new();
	GtkEventController* window_controller = gtk_event_controller_key_new();

	gtk_range_set_value(GTK_RANGE(volume_data->scale), volume * 100);

	gtk_widget_set_size_request(song_control->progress_bar, 600, 10);
	gtk_widget_set_size_request(volume_data->scale, 200, 20);

	set_control_box(control_button_box);

	gtk_widget_set_halign(progress_bar_box, GTK_ALIGN_CENTER);
	gtk_widget_set_halign(volume_data->icon, GTK_ALIGN_END);
	gtk_widget_set_vexpand(song_list, true);
	
	gtk_widget_set_can_focus(song_control->progress_bar, FALSE);

	bar_id = g_signal_connect(song_control->progress_bar, "value-changed", G_CALLBACK(on_timestamp_change), NULL);

	g_signal_connect(event_controller, "key-pressed", G_CALLBACK(on_key_pressed), song_control);
	g_signal_connect(window_controller, "key-pressed", G_CALLBACK(on_key_pressed), song_control);
	
	volume_bar_id = g_signal_connect(volume_data->scale, "value-changed", G_CALLBACK(on_volume_change), volume_data);
	g_signal_connect(song_control->play_button, "clicked", G_CALLBACK (toggle_playback_state), NULL);
	// g_signal_connect(pause_button, "clicked", G_CALLBACK(on_pause_button_click), NULL);
	g_signal_connect(song_control->open_button, "clicked", G_CALLBACK(on_open_button_click), window);
	g_signal_connect(song_list, "row-activated", G_CALLBACK(select_song), song_control);
	// g_signal_connect_after(controller, "released", G_CALLBACK(on_double_click), progress_bar);

	gtk_widget_add_controller(window, window_controller);
	gtk_widget_add_controller(song_list, event_controller);
	// gtk_widget_add_controller(window, event_controller);
	gtk_widget_add_tick_callback(song_control->progress_bar, progress_bar_tick, labels, NULL);
	
	GtkWidget* main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrollable_song_box), song_list);

	gtk_box_append(GTK_BOX(control_button_box), song_control->play_button);
	gtk_box_append(GTK_BOX(control_button_box), song_control->open_button);
	gtk_box_append(GTK_BOX(control_button_box), volume_data->icon);
	gtk_box_append(GTK_BOX(control_button_box), volume_data->scale);

	gtk_box_append(GTK_BOX(progress_bar_box), labels->start);
	gtk_box_append(GTK_BOX(progress_bar_box), song_control->progress_bar);
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
		log("failed to init engine from miniaudio", ERROR);
		std::abort();
	}
	auto app = adw_application_new("org.player.audio", G_APPLICATION_DEFAULT_FLAGS);

	g_signal_connect (app, "activate", G_CALLBACK (activate_cb), NULL);

	int result_code = g_application_run(G_APPLICATION (app), argc, argv);
	ma_engine_uninit(&engine);

	return result_code;
}