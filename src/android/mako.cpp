#include <memory>
#include <SDL.h>
#include <android/log.h>
#include "../config.h"
#include "jnihelper.h"
#include "dri.h"
#include "mako.h"
#include "mako_midi.h"
#include "fm/mako_ymfm.h"

namespace {

const int SAMPLE_RATE = 44100;

MAKO *g_mako;
SDL_mutex* fm_mutex;
std::unique_ptr<MakoYmfm> fm;

void audio_callback(void*, Uint8* stream, int len) {
	SDL_LockMutex(fm_mutex);
	fm->Process(reinterpret_cast<int16_t*>(stream), len/ 4);
	SDL_UnlockMutex(fm_mutex);
}

} // namespace

MAKO::MAKO(NACT* parent, const Config& config) :
	use_fm(config.use_fm),
	current_music(0),
	next_loop(0),
	nact(parent)
{
	g_mako = this;
	strcpy(amus, "AMUS.DAT");
	for (int i = 1; i <= 99; i++)
		cd_track[i] = 0;

	SDL_InitSubSystem(SDL_INIT_AUDIO);
	SDL_AudioSpec fmt;
	SDL_zero(fmt);
	fmt.freq = SAMPLE_RATE;
	fmt.format = AUDIO_S16;
	fmt.channels = 2;
	fmt.samples = 4096;
	fmt.callback = &audio_callback;
	if (SDL_OpenAudio(&fmt, NULL) < 0) {
		WARNING("SDL_OpenAudio: %s", SDL_GetError());
		use_fm = false;
	}
	fm_mutex = SDL_CreateMutex();
}

MAKO::~MAKO() {
	SDL_LockMutex(fm_mutex);
	SDL_CloseAudio();
	SDL_UnlockMutex(fm_mutex);
	SDL_DestroyMutex(fm_mutex);
	fm_mutex = nullptr;
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

void MAKO::play_music(int page)
{
	if (current_music == page)
		return;

	stop_music();

	JNILocalFrame jni(16);
	if (!jni.env())
		return;

	int track = page < 100 ? cd_track[page] : 0;
	if (track) {
		jmethodID mid = jni.GetMethodID("cddaStart", "(IZ)V");
		jni.env()->CallVoidMethod(jni.context(), mid, track + 1, next_loop ? 0 : 1);
	} else if (use_fm) {
		DRI dri;
		int size;
		uint8* data = dri.load(amus, page, &size);
		if (!data)
			return;

		SDL_LockMutex(fm_mutex);
		fm = std::make_unique<MakoYmfm>(SAMPLE_RATE, data, true);
		SDL_UnlockMutex(fm_mutex);
		SDL_PauseAudio(0);
	} else {
		auto midi = std::make_unique<MAKOMidi>(nact, amus);
		if (!midi->load_mml(page)) {
			WARNING("load_mml(%d) failed", page);
			return;
		}
		midi->load_mda(page);
		std::vector<uint8_t> smf = midi->generate_smf(next_loop);

		char path[PATH_MAX];
		snprintf(path, PATH_MAX, "%s/tmp.mid", SDL_AndroidGetInternalStoragePath());
		FILE* fp = fopen(path, "w");
		if (!fp) {
			WARNING("Failed to create temporary file");
			return;
		}
		fwrite(smf.data(), smf.size(), 1, fp);
		fclose(fp);

		jstring path_str = jni.env()->NewStringUTF(path);
		if (!path_str) {
			WARNING("Failed to allocate a string");
			return;
		}
		jmethodID mid = jni.GetMethodID("midiStart", "(Ljava/lang/String;Z)V");
		jni.env()->CallVoidMethod(jni.context(), mid, path_str, next_loop ? 0 : 1);
	}

	current_music = page;
	next_loop = 0;
}

void MAKO::stop_music()
{
	if (!current_music)
		return;

	JNILocalFrame jni(16);
	if (jni.env()) {
		if (current_music < 100 && cd_track[current_music]) {
			jmethodID cdda = jni.GetMethodID("cddaStop", "()V");
			jni.env()->CallVoidMethod(jni.context(), cdda);
		} else {
			jmethodID mid = jni.GetMethodID("midiStop", "()V");
			jni.env()->CallVoidMethod(jni.context(), mid);
		}
	}

	if (fm) {
		SDL_PauseAudio(1);
		SDL_LockMutex(fm_mutex);
		fm = nullptr;
		SDL_UnlockMutex(fm_mutex);
	}
	current_music = 0;
}

bool MAKO::check_music()
{
	if (!current_music)
		return false;
	if (fm) {
		int mark, loop;
		SDL_LockMutex(fm_mutex);
		fm->get_mark(&mark, &loop);
		SDL_UnlockMutex(fm_mutex);
		return !loop;
	}

	JNILocalFrame jni(16);
	if (!jni.env())
		return false;
	if (current_music < 100 && cd_track[current_music]) {
		jmethodID cdda = jni.GetMethodID("cddaCurrentPosition", "()I");
		return jni.env()->CallIntMethod(jni.context(), cdda) != 0;
	} else {
		jmethodID mid = jni.GetMethodID("midiCurrentPosition", "()I");
		return jni.env()->CallIntMethod(jni.context(), mid) != 0;
	}
}

void MAKO::get_mark(int* mark, int* loop)
{
	SDL_LockMutex(fm_mutex);
	if (fm) {
		fm->get_mark(mark, loop);
	} else {
		*mark = 0;
		*loop = 0;
	}
	SDL_UnlockMutex(fm_mutex);
}

void MAKO::play_pcm(int page, bool loop)
{
	WARNING("not implemented");
}

void MAKO::stop_pcm() {}

bool MAKO::check_pcm()
{
	return false;
}

void MAKO::select_synthesizer(bool use_fm_) {
	if (use_fm == use_fm_)
		return;
	int page = current_music;
	stop_music();
	use_fm = use_fm_;
	play_music(page);
}

extern "C" {

JNIEXPORT void JNICALL Java_io_github_kichikuou_system3_GameActivity_selectSynthesizer(
	JNIEnv *env, jobject cls, jboolean use_fm) {
	g_mako->select_synthesizer(use_fm);
}

} // extern "C"
