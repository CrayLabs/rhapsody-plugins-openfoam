# Installation

## Prerequisites

- Python >= 3.9
- A sourced [OpenFOAM](https://www.openfoam.com) installation (required to run cases; only needed to build the [`radexWrite` function object](building-the-function-object.md) if you want live field export)
- [Dragon](https://dragonhpc.github.io/dragon/doc/_build/html/index.html) (`dragonhpc`), used as the default RHAPSODY execution backend in the examples
- [RHAPSODY](https://radical-cybertools.github.io/rhapsody) (`rhapsody-py`)

## Installing the Python Plugin

The Python package lives under [`src/python`](https://github.com/CrayLabs/rhapsody-plugins-openfoam/tree/main/src/python) and is installed with `pip`/`setuptools` from the repository root:

```bash
git clone https://github.com/CrayLabs/rhapsody-plugins-openfoam.git
cd rhapsody-plugins-openfoam
pip install -e .
```

This installs the `rhapsody_plugins.openfoam` package along with its declared dependencies (`dragonhpc`, `rhapsody-py`).

!!! note
    The package is imported as `rhapsody_plugins.openfoam`, e.g. `import rhapsody_plugins.openfoam as rof`.

## Verifying the Install

```bash
python -c "import rhapsody_plugins.openfoam as rof; print(rof.CaseDefinition)"
```

## Next Steps

- [Build the `radexWrite` function object](building-the-function-object.md) if you want to stream live field data out of a running solver.
- Jump into the [Quick Start](quick-start.md) to run your first case.
