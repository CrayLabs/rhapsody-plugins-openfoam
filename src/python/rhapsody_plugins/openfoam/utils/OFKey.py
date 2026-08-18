from .OFTime import OFTimestring

def OFKey(id, field, time=None, rank=None):
    key = f"{id}_{field}"
    if time is not None:
        key += f"_{OFTimestring(time)}"
    if rank is not None:
        key += f"_{rank}"
    return key