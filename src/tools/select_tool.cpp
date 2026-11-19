#include "tools/select_tool.hpp"

namespace mazz {

const char* SelectTool::hint() const {
    return "click: select  Cmd+click: add/remove  drag: move  Shift+drag: copy  "
           "Option+drag corner: rotate  Delete: delete  Esc: select nothing";
}

} // namespace mazz
