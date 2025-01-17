from __future__ import absolute_import, division, print_function
import os
from io import StringIO
import time
import tempfile

from libtbx.utils import Sorry
from scitbx.array_family import flex
from libtbx import adopt_init_args
from libtbx import easy_run

from cctbx.geometry_restraints.manager import manager as standard_manager

harkcal = 627.50946900
bohrang = 0.52918

class base_manager():
  def __init__(self,
               atoms,
               method,
               basis_set,
               solvent_model,
               charge,
               multiplicity,
               preamble=None,
               log=None,
               ):
    adopt_init_args(self, locals())
    self.times = flex.double()
    self.energies = {}
    if self.preamble is None:
      self.preamble = os.path.basename(tempfile.NamedTemporaryFile().name)
    # validate
    if self.basis_set is None: self.basis_set=''
    if self.solvent_model is None: self.solvent_model=''

  def __repr__(self):
    program = getattr(self, 'program', '')
    if program: program = ' - %s' % program
    outl = 'QI Manager%s\n' % program
    outl += ' charge: %s multiplicity: %s\n method: %s basis: "%s" solvent: "%s"\n' % (
      self.charge,
      self.multiplicity,
      self.method,
      self.basis_set,
      self.solvent_model,
      )
    for atom in self.atoms:
      outl += '  %s\n' % atom.quote()
    return outl

  def get_charge(self): return self.charge

  def set_charge(self, charge): self.charge = charge

  def add_atoms(self, atoms, replace=False):
    if replace:
      self.atoms=atoms
    else:
      quotes = []
      for atom in self.atoms:
        quotes.append(atom.quote())
      for atom in atoms:
        assert atom.quote() not in quotes, 'found duplicate %s' % atom.quote()
      self.atoms.append(atoms)

  def set_interest_atoms(self, selection_array):
    assert len(selection_array)==len(self.atoms)
    self.interest_array = selection_array

  def set_frozen_atoms(self, selection_array):
    assert len(selection_array)==len(self.atoms)
    self.freeze_a_ray = selection_array

  def get_opt(self, cleanup=False, file_read=False):
    import random
    rc = []
    for atom in self.atoms:
      rc.append([])
      for i in range(3):
        rc[-1].append(atom.xyz[i]+(random.random()-0.5)/10)
    tmp = []
    if hasattr(self, 'interest_array'):
      for sel, atom in zip(self.interest_array, rc):
        if sel:
          tmp.append(atom)
      rc=tmp
    return flex.vec3_double(rc)

  def get_timings(self, energy=False):
    return '-'

#
# QM runner
#
def qm_runner(qmm,
              cleanup=True,
              file_read=False,
              verbose=False):
  if verbose: print(qmm)
  if qmm.program=='test':
    func = base_manager.get_opt
  elif qmm.program=='orca':
    func = orca_manager.get_opt
  else:
    raise Sorry('QM program not found or set "%s"' % qmm.program)
  xyz = func(qmm, cleanup=cleanup, file_read=file_read)
  return xyz
#
# ORCA
#
'''
                                .--------------------.
          ----------------------|Geometry convergence|-------------------------
          Item                value                   Tolerance       Converged
          ---------------------------------------------------------------------
          Energy change      -0.2772205978            0.0000050000      NO
          RMS gradient        0.0273786248            0.0001000000      NO
          MAX gradient        0.1448471259            0.0003000000      NO
          RMS step            0.0097797205            0.0020000000      NO
          MAX step            0.0482825340            0.0040000000      NO
          ........................................................
          Max(Bonds)      0.0256      Max(Angles)    0.97
          Max(Dihed)        0.59      Max(Improp)    0.00
          ---------------------------------------------------------------------

          ----------------------------------------------------------------------------
                                  WARNING !!!
       The optimization did not converge but reached the maximum number of
       optimization cycles.
       Please check your results very carefully.
    ----------------------------------------------------------------------------
          '''
def run_qm_cmd(cmd,
               log_filename,
               error_lines=None,
               verbose=False):
  if verbose: print('run_qm_cmd',cmd,log_filename)
  local_logger = ''
  rc = easy_run.go(cmd)
  error_line=None
  for line in rc.stdout_lines:
    local_logger += '%s\n' % line
    if line.find('GEOMETRY OPTIMIZATION CYCLE')>-1:
      print(line)
    if error_lines:
      for el in error_lines:
        if local_logger.find(el)>-1:
          error_line = line
          break
    if error_line: break
  if rc.stderr_lines:
    print('stderr')
    for line in rc.stderr_lines:
      print(line)
    print('stdout')
    for line in rc.stdout_lines:
      print(line)
    assert 0
  f=open(log_filename, 'w')
  f.write(local_logger)
  del f
  if error_line:
    raise Sorry(error_line)
  return rc

class orca_manager(base_manager):

  def set_sites_cart(self, sites_cart):
    assert len(self.atoms)==len(sites_cart)
    for atom, site_cart in zip(self.atoms, sites_cart):
      atom.xyz = site_cart

  def read_engrad_output(self):
    '''#
# Number of atoms
#
 5
#
# The current total energy in Eh
#
    -49.737578240166
#
# The current gradient in Eh/bohr
#
       0.009609074575
       0.007643624367
      -0.019142934602
       0.010258288141
      -0.020537435105
      -0.000346851479
       0.000773577750
       0.021293697927
       0.011393000407
      -0.018928466970
      -0.006660132835
       0.008456622796
      -0.001712473496
      -0.001739754355
      -0.000359837122
#
# The atomic numbers and current coordinates in Bohr
#
   8    59.0407136   72.7582356   32.5750991
   8    57.8558553   75.8403789   29.3417777
   8    58.8800869   71.4618835   28.1663680
   8    62.2022254   74.3474953   29.5553167
  16    59.4829095   73.6048329   29.8973572'''
    f=open('orca_%s.engrad' % self.preamble, 'r')
    lines = f.read()
    del f
    lines = lines.split('#')

    energy = None
    gradients = flex.vec3_double()
    for line in lines[6].splitlines():
      if len(line.strip()):
        energy = float(line)
        break
    tmp=[]
    for line in lines[9].splitlines():
      if len(line.strip()):
        tmp.append(float(line)*harkcal*bohrang)
        if len(tmp)==3:
          gradients.append(tmp)
          tmp=[]

    self.energy = energy
    self.gradients = gradients
    return self.energy, self.gradients

  def get_coordinate_filename(self):
    filename = 'orca_%s.xyz' % self.preamble
    return filename

  def read_xyz_output(self):
    filename = self.get_coordinate_filename()
    if not os.path.exists(filename):
      raise Sorry('QM output filename not found: %s' % filename)
    f=open(filename, 'r')
    lines = f.read()
    del f
    rc = flex.vec3_double()
    for i, line in enumerate(lines.splitlines()):
      if i>=2:
        tmp = line.split()
        rc.append((float(tmp[1]), float(tmp[2]), float(tmp[3])))
    return rc

  def write_input(self, outl):
    f=open('orca_%s.in' % self.preamble, 'w')
    f.write(outl)
    del f

  def get_cmd(self):
    # cmd = '%s orca_%s.in >& orca_%s.log' % (
    cmd = '%s orca_%s.in' % (
      os.environ['PHENIX_ORCA'],
      self.preamble,
      # self.preamble,
      )
    return cmd

  def run_cmd(self):
    t0=time.time()
    cmd = self.get_cmd()
    run_qm_cmd(cmd,
               'orca_%s.log' % self.preamble,
               error_lines=[
                  'ORCA finished by error termination in GSTEP',
                  '-> impossible',
                  'SCF NOT CONVERGED AFTER',
               ],
               )
    self.times.append(time.time()-t0)

  def get_coordinate_lines(self):
    outl = '* xyz %s %s\n' % (self.charge, self.multiplicity)
    for i, atom in enumerate(self.atoms):
      outl += ' %s %0.5f %0.5f %0.5f # %s %s\n' % (
        atom.element,
        atom.xyz[0],
        atom.xyz[1],
        atom.xyz[2],
        atom.id_str(),
        i,
        )
    outl += '*\n'
    return outl

  def get_timings(self, energy=None):
    if not self.times: return '-'
    f='  Timings : %0.2fs (%ss)' % (
      self.times[-1],
      self.times.format_mean(format='%.2f'))
    if energy:
      f+=' Energy : %0.6f' % energy
    return f

  def get_engrad(self):
    outl = '! %s %s %s EnGrad\n\n' % (self.method,
                                      self.basis_set,
                                      self.solvent_model)
    outl += self.get_coordinate_lines()
    if outl in self.energies:
      self.times.append(0)
      return self.energies[outl]
    self.write_input(outl)
    self.run_cmd()
    energy, gradients = self.read_engrad_output()
    self.print_timings(energy)
    self.energies[outl] = (energy, gradients)
    return energy, gradients

  def opt_setup(self):
    outl = '! %s %s %s Opt\n\n' % (self.method,
                                   self.basis_set,
                                   self.solvent_model)
    outl += self.get_coordinate_lines()
    if hasattr(self, 'freeze_a_ray'):
      freeze_outl = '''%geom
      Constraints
'''
      if hasattr(self, 'freeze_a_ray'):
        for i, (sel, atom) in enumerate(zip(self.freeze_a_ray, self.atoms)):
          if sel:
            freeze_outl += '{C %d C} # Restraining %s\n' % (i, atom.id_str())
      freeze_outl += 'end\nend\n'
      outl += freeze_outl
    self.write_input(outl)

  def get_opt(self, cleanup=False, file_read=True):
    coordinates = None
    if file_read:
      filename = self.get_coordinate_filename()
      if os.path.exists(filename):
        print('  Reading coordinates from %s\n' % filename)
        coordinates = self.read_xyz_output()
    if coordinates is None:
      self.opt_setup()
      self.run_cmd()
      coordinates = self.read_xyz_output()
    if cleanup: self.cleanup(level=cleanup)
    if hasattr(self, 'interest_array'):
      tmp = []
      if hasattr(self, 'interest_array'):
        for sel, atom in zip(self.interest_array, coordinates):
          if sel:
            tmp.append(atom)
        coordinates=tmp
    return flex.vec3_double(coordinates)

  def cleanup(self, level=None, verbose=False):
    if not self.preamble: return
    if level is None: return
    #
    tf = 'orca_%s.trj' % self.preamble
    if os.path.exists(tf):
      uf = 'orca_%s_trj.xyz' % self.preamble
      print('rename',tf,uf)
      os.rename(tf, uf)
    most_keepers = ['.xyz', '.log', '.in', '.engrad', '.trj']
    for filename in os.listdir('.'):
      if filename.startswith('orca_%s' % self.preamble):
        if level=='most':
          name, ext = os.path.splitext(filename)
          if ext in most_keepers: continue
        if verbose: print('  removing',filename)
        os.remove(filename)

  def view(self, cmd, ext='.xyz'):
    # /Applications/Avogadro.app/Contents/MacOS/Avogadro
    print(cmd)
    tf = 'orca_%s' % self.preamble
    print(tf)
    filenames =[]
    for filename in os.listdir('.'):
      if filename.startswith(tf) and filename.endswith(ext):
        filenames.append(filename)
    filenames.sort()
    print(filenames)
    cmd += ' %s' % filenames[-1]
    easy_run.go(cmd)

class manager(standard_manager):
  def __init__(self,
               params,
               log=StringIO()):
    # self.gradients_factory = gradients_factory
    adopt_init_args(self, locals(), exclude=["log"])
    self.validate()

  def validate(self):
    qi = self.params.qi
    assert qi.use_quantum_interface
    assert qi.selection
    if qi.orca.use_orca:
      print('Orca')

  def get_engrad(self, sites_cart):
    self.execution_manager.set_sites_cart(sites_cart)
    return self.execution_manager.get_engrad()

  def get_opt(self, sites_cart):
    self.execution_manager.set_sites_cart(sites_cart)
    return self.execution_manager.get_opt()

  def set_qm_info(self,
                  method,
                  basis_set,
                  solvent_model,
                  charge,
                  multiplicity,
                  ):
    adopt_init_args(self, locals())
    if self.basis_set is None:
      self.basis_set = ''
    if self.solvent_model is None:
      self.solvent_model = ''
    self.execution_manager = orca_manager( self.qm_atoms,
                                           self.method,
                                           self.basis_set,
                                           self.solvent_model,
                                           self.charge,
                                           self.multiplicity
                                           )

  def set_qm_atoms(self, qm_atoms):
    self.qm_atoms = qm_atoms
    self.qm_iseqs = []
    for atom in self.qm_atoms:
      self.qm_iseqs.append(atom.i_seq)

  def energies_sites(self,
                     sites_cart,
                     flags=None,
                     custom_nonbonded_function=None,
                     compute_gradients=False,
                     gradients=None,
                     disable_asu_cache=False,
                     normalization=False,
                     external_energy_function=None,
                     extension_objects=[],
                     site_labels=None,
                     log=None):
    result = standard_manager.energies_sites(
      self,
      sites_cart,
      flags=flags,
      custom_nonbonded_function=custom_nonbonded_function,
      compute_gradients=compute_gradients,
      gradients=gradients,
      disable_asu_cache=disable_asu_cache,
      normalization=normalization,
      external_energy_function=external_energy_function,
      extension_objects=extension_objects,
      site_labels=site_labels,
      )
    if compute_gradients:
      qm_sites_cart = []
      for i_seq in self.qm_iseqs:
        qm_sites_cart.append(sites_cart[i_seq])
      # coordinates = self.get_opt(qm_sites_cart)
      # print(list(coordinates))
      # assert 0
      energy, gradients = self.get_engrad(qm_sites_cart)
      for i_seq, gradient in zip(self.qm_iseqs, gradients):
        result.gradients[i_seq]=gradient
    return result

def main():
  from iotbx import pdb
  pdb_lines = '''
HETATM   97  S   SO4 A  13      31.477  38.950  15.821  0.50 25.00           S
HETATM   98  O1  SO4 A  13      31.243  38.502  17.238  0.50 25.00           O
HETATM   99  O2  SO4 A  13      30.616  40.133  15.527  0.50 25.00           O
HETATM  100  O3  SO4 A  13      31.158  37.816  14.905  0.50 25.00           O
HETATM  101  O4  SO4 A  13      32.916  39.343  15.640  0.50 25.00           O
'''
  pdb_inp = pdb.input(lines=pdb_lines, source_info='lines')
  qi_grm = orca_manager(pdb_inp.atoms(),
                        'PM3',
                        '',
                        '',
                        -2,
                        1,
                        preamble='test',
                        )
  print(qi_grm)
  energy, gradients = qi_grm.get_engrad()
  print(energy, list(gradients))
  coordinates = qi_grm.get_opt()
  print(list(coordinates))

if __name__ == '__main__':
  main()
