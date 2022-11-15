#pragma once
#include <archive.h>
#include <archive_entry.h>
#include <cstddef>
#include <memory>
#include <map>
#include <utility>
#include "assert_dbg.h"
#include "cursor.h"
#include "daw_async_task.h"
#include "file/projectfile.h"
#include "host/audiocache/audiocache.h"
#include "host/audiocache/audiocache.h"
#include "host/audiocache/samplefileidx.h"
#include "host/clip/clip.h"
#include "host/daw/mainctrl.h"
#include "host/host.h"
#include "str_util.h"
#include "types.h"
#include "window_impl.h"

namespace DAW {

struct samplefile_index_incremental_loader_t {
    audiocache* cache = nullptr;
    struct archive* ar = nullptr;
    samplefile_index_t index;
    size_t indexSize;
    size_t indexPos = 0;
    String workingDir;
    bool bFinished = false;
    struct archive_entry* entry = nullptr;
    String curFileName;
    samplefile_index_incremental_loader_t(audiocache* cache, struct archive* ar, samplefile_index_t& index, String workingDir)
        : cache(cache),
        ar(ar),
        index(index),
        indexSize(index.list.size()),
        workingDir(std::move(workingDir)) {
    }
    double getProgress() const {
        return !indexSize ? 1.0 : indexPos / double(indexSize);
    }
    void step() {
        if (ar) {
            if (archive_read_next_header(ar, &entry) == ARCHIVE_OK) {
                curFileName = archive_entry_pathname(entry);
            } else {
                bFinished = true;
                archive_read_free(ar);
            }
        } else {
            if (indexPos < index.list.size()) {
                curFileName = index.list[indexPos].name;
            } else {
                bFinished = true;
            }
        }
    }
    void loadSingleStep() {
        if (bFinished) {
            return;
        }
        if (curFileName.empty()) {
            step();
            return;
        }
        if (bFinished) {
            return;
        }
        if (ar) {
            auto it = index.list.begin();
            const auto itEnd = index.list.end();
            for (; it != itEnd; ++it) {
                if (it->name == curFileName) {
                    cache->loadFile(it->name, it->id, workingDir, ar, entry);
                    it = index.list.erase(it);
                    indexPos++;
                    curFileName = "";
                    return;
                }
            }
            curFileName = "";
        } else {
            auto& fileIndex = index.list[indexPos];
            cache->loadFile(curFileName, fileIndex.id, workingDir, nullptr, nullptr);
            curFileName = "";
            indexPos++;
        }
    }
    bool isFinished() const {
        return bFinished;
    }
};

struct load_project_task final : public async_task_t {
    DawInstance* daw;
    std::shared_ptr<project_to_load_t> projectToLoad;
    int32_t step = 0;
    int32_t substep = 0;
    int32_t numSubsteps = 0;
    String taskDesc;
    String progressDesc;
    bool projectLoadErrored = false;
    std::vector<effectbase*> pluginsDeferred; 
    std::shared_ptr<DAW::samplefile_index_incremental_loader_t> sampleLoader;
    load_project_task(DawInstance* daw, std::shared_ptr<project_to_load_t>&& projectToLoad)
        : daw(daw)
        , projectToLoad(projectToLoad) {
        String projectFileName;
        SplitPath(projectToLoad->projectfile->path, nullptr, &projectFileName, nullptr);
        taskDesc = "Load " + projectFileName;
        progressDesc = "";
    }
    String getTaskName() const override {
        return taskDesc;
    }
    String getProgressDesc() const override {
        return progressDesc;
    }
    void run() override;
    void getPreciseProgress(double& progressOverall, double& progressDetail) override;
};

} // namespace DAW
