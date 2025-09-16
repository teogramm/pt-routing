#include "raptor/state.h"

namespace raptor {
    std::optional<LabelManager::LabelType> LabelManager::get_label(const Stop& stop, const LabelContainer& labels) {
        const auto label = labels.find(stop);
        return label == labels.end() ? std::nullopt : std::make_optional(label->second);
    }

    void LabelManager::new_round() {
        previous_round_labels = current_round_labels;
    }

    std::optional<LabelManager::LabelType> LabelManager::get_latest_label(const Stop& stop) const {
        return get_label(stop, current_round_labels);
    }

    std::optional<LabelManager::LabelType> LabelManager::get_previous_label(const Stop& stop) const {
        return get_label(stop, previous_round_labels);
    }
}

