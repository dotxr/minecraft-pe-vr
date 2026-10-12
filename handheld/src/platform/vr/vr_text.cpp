#include "vr_text.h"

#include "../../client/gui/screens/DialogDefinitions.h"

namespace vr {
namespace text {
namespace {

struct Request {
	bool open;
	int status;
	int index;
	std::vector<std::string> prompts;
	std::vector<std::string> values;
};

Request& request() {
	static Request r = {false, -1, 0, std::vector<std::string>(), std::vector<std::string>()};
	return r;
}

// Dummy slot returned when no field is open, so callers can always take a reference.
std::string gNoValue;

}  // namespace

void begin(int dialogId) {
	Request& r = request();
	r.open = true;
	r.status = -1;
	r.index = 0;
	r.prompts.clear();
	r.values.clear();

	if (dialogId == DialogDefinitions::DIALOG_CREATE_NEW_WORLD) {
		r.prompts.push_back("World name");
		r.prompts.push_back("Seed (empty for random)");
		r.prompts.push_back("Mode: survival, or empty");
	} else if (dialogId == DialogDefinitions::DIALOG_RENAME_MP_WORLD) {
		r.prompts.push_back("World name");
	} else {
		r.prompts.push_back("Text");
	}
	r.values.resize(r.prompts.size());
}

bool open() {
	return request().open;
}

int status() {
	return request().status;
}

std::vector<std::string> result() {
	return request().values;
}

std::string& currentValue() {
	Request& r = request();
	if (!r.open || r.index >= (int)r.values.size()) {
		gNoValue.clear();
		return gNoValue;
	}
	return r.values[r.index];
}

const char* currentPrompt() {
	Request& r = request();
	if (!r.open || r.index >= (int)r.prompts.size()) return "";
	return r.prompts[r.index].c_str();
}

void advance() {
	Request& r = request();
	if (!r.open) return;
	if (r.index + 1 < (int)r.prompts.size()) {
		++r.index;
		return;
	}
	r.open = false;
	r.status = 1;
}

void cancel() {
	Request& r = request();
	if (!r.open) return;
	r.open = false;
	r.status = 0;
}

}  // namespace text
}  // namespace vr
