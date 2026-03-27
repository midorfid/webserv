import time
import sys

def LogTime(func):
    def wrapper(*args, **kwargs):
        start_time = time.perf_counter()

        result = func(*args, **kwargs)

        end_time = time.perf_counter()

        print(f"TRACE: {func.__name__} took {end_time - start_time:.6f}s", file=sys.stderr)

        return result
    return wrapper
