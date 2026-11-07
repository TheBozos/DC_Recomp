"""Opt-in, image-scoped probes in generated C++; instructions remain untouched.

Match every emitted register transfer against the same compiler's ordered facts.
A mismatch stops the build rather than instrumenting a guessed source address.
"""
import argparse
import json
import pathlib
import re


def instrument(text, functions, image):
    body = re.compile(r'(static void body_0x([a-f0-9]+)\(.*?)(?=\nvoid \w+_)', re.S)
    expression = re.compile(r'const uint32_t target = BZ_W\((\d+)\);')
    count = 0

    def replace_body(match):
        nonlocal count
        facts = functions[int(match[2], 16)]['sites']
        emitted = list(expression.finditer(match[1]))
        if len(emitted) != len(facts) or any(int(m[1]) != s['reg'] for m, s in zip(emitted, facts)):
            raise ValueError(f'indirect facts/emission mismatch in {match[2]}')
        it = iter(facts)

        def replace_expr(expr):
            nonlocal count
            site = next(it)
            if site['kind'] == 'return':
                return expr[0]
            count += 1
            return f'const uint32_t target = dcTraceIndirect(rt, ctx, {image}u, 0x{site["pc"]:x}u, BZ_W({site["reg"]}));'
        return expression.sub(replace_expr, match[1])

    output = body.sub(replace_body, text)
    return '#include "dc_indirect_trace.h"\n' + output, count


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=pathlib.Path)
    parser.add_argument('image', choices=('dc', 'title', 'dun'))
    args = parser.parse_args()
    functions = {f['start']: f for f in json.loads((args.directory / (args.image + '.indirect.json')).read_text())['functions']}
    total = 0
    for path in args.directory.glob(args.image + '_[0-9]*.cpp'):
        output, count = instrument(path.read_text(), functions, ('dc', 'title', 'dun').index(args.image))
        path.write_text(output); total += count
    expected = sum(s['kind'] != 'return' for f in functions.values() for s in f['sites'])
    if total != expected:
        raise ValueError(f'incomplete image instrumentation: {total} of {expected} sites')
    print(f'{args.image}: {total} indirect probes (audit build only)')


if __name__ == '__main__':
    main()
