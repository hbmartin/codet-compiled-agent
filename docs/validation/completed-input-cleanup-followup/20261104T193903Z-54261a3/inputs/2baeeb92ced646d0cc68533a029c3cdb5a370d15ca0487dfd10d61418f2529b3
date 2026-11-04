"""Checked Clang plist file-index resolution, shared with older baseline runners."""
def resolve_files(value, files):
    if not isinstance(files, list) or any(not isinstance(name, str) for name in files):
        raise ValueError('Invalid diagnostic file registry')
    def visit(item):
        if isinstance(item, dict):
            result = {}
            for key, child in item.items():
                if key == 'file' and isinstance(child, int):
                    if type(child) is not int or not 0 <= child < len(files):
                        raise ValueError('Invalid diagnostic file index')
                    result[key] = files[child]
                else:
                    result[key] = visit(child)
            return result
        if isinstance(item, list):
            return [visit(child) for child in item]
        return item
    return visit(value)
