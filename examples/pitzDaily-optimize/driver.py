import asyncio
import multiprocessing as mp

from rhapsody.backends import DragonExecutionBackend

import rhapsody_plugins.openfoam as rof
from dragon.data.ddict import DDict
from radex import DragonClient as Client

import random


def _modify_dictionaries(reg, parameters, run_path, id):
    epsilonDict = "0/epsilon"
    turbulenceDict = "constant/turbulenceProperties"
    controlDict = "system/controlDict"

    # Modify epsilion boundary conditions
    epsilon = parameters["epsilon"]
    options = {
        "entry": f"internalField",
        "set": f"uniform {epsilon}"
    }
    reg.foamDictionary(args=[epsilonDict], options=options).run_local(cwd=run_path)

    for patch in ["inlet", "upperWall", "lowerWall"]:
        options = {
            "entry": f"boundaryField.{patch}.value",
            "set": f"uniform {epsilon}"
        }
        reg.foamDictionary(args=[epsilonDict], options=options).run_local(cwd=run_path)

    # Modify k-epsilon parameters
    for coeff in ["Cmu", "C1", "C2", "sigmaEps"]:
        options = {
            "entry": f"RAS.{coeff}",
            "set": parameters[coeff]
        }
        reg.foamDictionary(args=[turbulenceDict], options=options).run_local(cwd=run_path)

    # Modify the radex writer id
    options = {
        "entry": "functions.radexWrite.identifier",
        "set": id
    }
    reg.foamDictionary(args=[controlDict], options=options).run_local(cwd=run_path)


def create_openfoam_case(reg, radex_store, parameters, id):
    case = rof.CaseDefinition("../openfoam-cases/pitzDaily", f"./run-{id}", clean=True)

    _modify_dictionaries(reg, parameters, case.run_path, id)
    simplefoam = reg.simpleFoam(execute_async=True, radex_store=radex_store)
    case.add_stage("solve", simplefoam)

    return case

def generate_parameters():
    return {
        "epsilon": random.uniform(10.,20.),
        "Cmu": random.uniform(0.01, 0.2),
        "C1": random.uniform(1.0, 2.0),
        "C2": random.uniform(1.5, 2.5),
        "sigmaEps": random.uniform(1.1,1.5),
    }

async def main():

    reg = rof.OFExecutableRegistry()
    mp.set_start_method("dragon", force=True)

    radex_store = DDict(
        managers_per_node=1,
        n_nodes=1,
        total_mem=512 * 1024 * 1024,
        wait_for_keys=True,
        working_set_size=2 + 2,
    )

    backend = await DragonExecutionBackend()

    async with rof.OFSession(backends=[backend]) as session:
        cases = [
            create_openfoam_case(reg, radex_store, generate_parameters(), f"pD-{e:04d}")
            for e in range(4)
        ]
        ensemble_tasks = []
        for case in cases:
            ensemble_tasks.extend(await case.stages["solve"].execute(session))

        await asyncio.gather(*ensemble_tasks)

    c = Client(radex_store.serialize(), timeout=5)
    s = c.get_scalar("pD-0000_final_step")
    key = f"pD-0000_avgInlets_{int(s)}_0"
    print(key)
    print(c.get_scalar(key))


if __name__ == "__main__":
    asyncio.run(main())
