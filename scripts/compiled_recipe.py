"""Canonical compiled dependency inputs; distribution-only fields are excluded."""


def compiled_recipe(lock):
    if not isinstance(lock, dict) or type(lock.get("format")) is not int or lock["format"] != 1:
        raise ValueError("Unsupported Tiger dependency lock format")
    if any(not isinstance(lock.get(key), list) for key in ("dependencies", "patches")):
        raise ValueError("Invalid Tiger dependency recipe")
    return {key: lock[key] for key in ("dependencies", "patches")}
