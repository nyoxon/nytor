#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "plugins/plugin.h"
#include "util/files.h"
#include "terminal/input.h"
#include "terminal/parser.h"

struct plugin_result plugin_language_load(const char* name) {
	struct plugin_result result = {
		NULL,
		0
	};

	char path[PATH_MAX_LENGTH];

	if (make_plugin_language_path(path, PATH_MAX_LENGTH, name) < 0) {
		return result;
	}

	void* plugin = dlopen(path, RTLD_NOW);

	if (!plugin) {
		result.error = 1;
	} else {
		result.handle = plugin;
	}

	return result;
}

void plugin_destroy(void* plugin) {
	if (!plugin) {
		return;
	}

	dlclose(plugin);
}


// a language plugin was detected in the configuration file,
// but an error ocurred while loading it.
// informs the user that an error of this type has ocurred
// and asks whether they want to continue anyway or exit
// the program
int handle_load_error
(
	const struct plugin_result* result,
	const char* name
)
{
	clean_terminal();
	fprintf(stderr, "failed to load language plugin: %s\n", name);

	if (result->error) {
		fprintf(stderr, "reason: %s\n\n", dlerror());
	}

	fprintf(stderr, "continue without it?\n(y/n)\n");
	enable_raw_mode();

	while (1) {
		struct event event = parser_read_key();

		if (event.type != EVENT_KEY) {
			continue;
		}

		int key = event.key.content;

		if (key == 'y' || key == 'Y') {
			break;
		}

		if (key == 'n' || key == 'N') {
		   disable_raw_mode();
		   return -1;
		}
	}

	disable_raw_mode();
	return 0;
}

static void vector_plugin_destroy(void* ptr) {
	LangPluginData* data = (LangPluginData*) ptr;

	if (data->lexer) {
		data->lexer->destroy(data->lexer);
	}

	if (data->handle) {
		plugin_destroy(data->handle);
	}
}

int load_lang_plugins(Vector* out, Vector* plugin_names) {
	vector_init(out, sizeof(LangPluginData), vector_plugin_destroy);

	int ret = 0;

	if (plugin_names->size == 0) {
		return ret;
	}

	for (size_t i = 0; i < plugin_names->size; i++) {
		char** name = vector_get(plugin_names, i);

		struct plugin_result result = plugin_language_load(*name);
		void* handle = result.handle;

		int failed = 0;

		if (!handle && strlen(*name) > 0) {
			failed = 1;

			if (handle_load_error(&result, *name) < 0) {
				ret = -1;
				break;
			}
		}

		if (failed || !handle) {
			continue;
		}

		plugin_init_t init = dlsym(handle, "plugin_init");

		if (!init) {
			plugin_destroy(handle);

			ret = -1;
			break;
		}

		const struct language_plugin* lang_plugin = init();

		if (!lang_plugin) {
			plugin_destroy(handle);

			ret = -1;
			break;
		}

		struct lexer* lexer = lang_plugin->create_lexer();

		LangPluginData data = { handle, lexer, lang_plugin, };

		vector_push(out, &data);
	}

	if (ret < 0) {
		vector_free(out);
	}

	vector_free(plugin_names);
	return ret;
}

void destroy_lang_plugins(Vector* lang_plugins) {
	vector_free(lang_plugins);
}