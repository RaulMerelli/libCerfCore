#pragma once

#include <mutex>
#include <vector>

namespace cerfcore {

class StateReader;
class StateWriter;

template <typename Source>
class SourceSelection {
public:
    virtual ~SourceSelection() = default;

    void                 Register(Source* src);
    std::vector<Source*> Sources() const;
    Source*              Active() const;
    void                 SetActive(Source* src);
    void                 CycleNext();
    void                 ReevaluateAuto();
    void                 RearmAutoSelect();

protected:
    void SaveSelection(StateWriter& w) const;
    void RestoreSelection(StateReader& r);

    virtual void SelectionChanged() {}

private:
    template <typename Mutate>
    void Update(Mutate mutate);
    void SelectAutoLocked();

    mutable std::mutex   selection_mtx_;
    std::vector<Source*> sources_;
    Source*              active_      = nullptr;
    bool                 user_picked_ = false;
};

}
