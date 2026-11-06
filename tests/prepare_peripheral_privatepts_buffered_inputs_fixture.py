"""Load the production command wrapper without executing the generator CLI."""
import importlib.util
import pathlib

path = pathlib.Path(__file__).resolve().parents[1] / 'scripts/prepare-peripheral-privatepts-buffered-inputs.py'
spec = importlib.util.spec_from_file_location('buffered_input_generator', path)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
root_command = module.root
