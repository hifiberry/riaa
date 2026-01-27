#!/usr/bin/env python3
"""
LADSPA Plugin Frequency Response Tester

Tests LADSPA plugins by sweeping through frequencies and measuring
the output amplitude to generate frequency response curves.
"""

import sys
import subprocess
import math
import re
import tempfile
import argparse
from pathlib import Path

try:
    import matplotlib.pyplot as plt
    import matplotlib
    matplotlib.use('Agg')  # Non-interactive backend
    HAS_MATPLOTLIB = True
except ImportError:
    HAS_MATPLOTLIB = False
    print("Warning: matplotlib not available, no plots will be generated", file=sys.stderr)


class PluginTester:
    def __init__(self):
        # Test parameters (defaults, can be overridden by argparse)
        self.f_min = 5
        self.f_max = 50000
        self.steps_per_oct = 12
        
        # Audio generation parameters
        self.duration = 1.0
        self.trim_start = 0.2
        self.trim_len = 0.6
        
        # Plugin info
        self.plugin_path = None
        self.plugin_label = None
        self.plugin_name = None
        self.num_inputs = 0
        self.num_outputs = 0
        self.parameters = []
        self.param_names = []
        self.no_plot = False
        self.y_min = None
        self.y_max = None
    
    def parse_args(self, argv):
        """Parse command line arguments using argparse."""
        parser = argparse.ArgumentParser(
            description='Test LADSPA plugins and generate frequency response curves',
            formatter_class=argparse.RawDescriptionHelpFormatter,
            epilog='''
Examples:
  %(prog)s riaa --params 1 1 0
  %(prog)s riaa --f-min 20 --f-max 20000 --steps 24
            '''
        )
        
        parser.add_argument('plugin', 
                          help='Plugin name (without .so extension)')
        
        parser.add_argument('--params', '-p',
                          nargs='*',
                          default=[],
                          metavar='VALUE',
                          help='Plugin parameters (control values). For RIAA: Gain Subsonic RIAA-Enable Declick-Enable Spike-Threshold Spike-Width Store-Settings')
        
        parser.add_argument('--output', '-o',
                          metavar='FILE',
                          help='Output filename (default: <plugin>_response.txt)')
        
        parser.add_argument('--f-min',
                          type=float,
                          default=5,
                          metavar='HZ',
                          help='Minimum test frequency in Hz (default: 5)')
        
        parser.add_argument('--f-max',
                          type=float,
                          default=50000,
                          metavar='HZ',
                          help='Maximum test frequency in Hz (default: 50000)')
        
        parser.add_argument('--steps',
                          type=int,
                          default=12,
                          metavar='N',
                          help='Steps per octave (default: 12)')
        
        parser.add_argument('--duration',
                          type=float,
                          default=1.0,
                          metavar='SEC',
                          help='Test signal duration in seconds (default: 1.0)')
        
        parser.add_argument('--y-min',
                          type=float,
                          metavar='DB',
                          help='Y-axis minimum in dB (default: auto-scale with 10%% headroom)')
        
        parser.add_argument('--y-max',
                          type=float,
                          metavar='DB',
                          help='Y-axis maximum in dB (default: auto-scale with 10%% headroom)')
        
        parser.add_argument('--no-plot',
                          action='store_true',
                          help='Skip generating plot')
        
        args = parser.parse_args(argv)
        
        # Set parameters from args
        plugin_name = args.plugin
        self.plugin_path = f"/usr/local/lib/ladspa/{plugin_name}.so"
        
        # Check if plugin exists
        if not Path(self.plugin_path).exists():
            parser.error(f"Plugin not found: {self.plugin_path}")
        
        # Handle parameters - split comma-separated values
        params_list = []
        for p in args.params:
            if ',' in str(p):
                # Split comma-separated values
                params_list.extend(str(p).split(','))
            else:
                params_list.append(str(p))
        self.parameters = params_list
        self.output_file = args.output if args.output else f"{plugin_name}_response.txt"
        
        # Set test parameters
        self.f_min = args.f_min
        self.f_max = args.f_max
        self.steps_per_oct = args.steps
        self.duration = args.duration
        self.no_plot = args.no_plot
        self.y_min = args.y_min
        self.y_max = args.y_max
        
        return plugin_name
    
    def analyze_plugin(self):
        """Get plugin information using analyseplugin."""
        try:
            result = subprocess.run(
                ['analyseplugin', self.plugin_path],
                capture_output=True,
                text=True,
                check=True
            )
            output = result.stdout
        except subprocess.CalledProcessError:
            print(f"Error: Failed to analyze plugin {self.plugin_path}")
            sys.exit(1)
        
        # Extract plugin name and label
        for line in output.split('\n'):
            if 'Plugin Name:' in line:
                self.plugin_name = line.split('"')[1] if '"' in line else line.split(':')[1].strip()
            elif 'Plugin Label:' in line:
                self.plugin_label = line.split('"')[1] if '"' in line else line.split(':')[1].strip()
        
        # Count audio input and output ports
        self.num_inputs = output.count('input, audio')
        self.num_outputs = output.count('output, audio')
        
        if self.num_inputs == 0:
            self.num_inputs = 1
        if self.num_outputs == 0:
            self.num_outputs = 1
        
        # Extract control port names
        self.param_names = []
        for line in output.split('\n'):
            if 'input, control' in line:
                match = re.search(r'"([^"]+)"', line)
                if match:
                    self.param_names.append(match.group(1))
    
    def print_plugin_info(self):
        """Print plugin information."""
        print("=== Plugin Analysis ===")
        print(f'Plugin Name: "{self.plugin_name}"')
        print(f'Plugin Label: "{self.plugin_label}"')
        print(f"Audio Channels: {self.num_inputs} input(s), {self.num_outputs} output(s)")
        print()
        
        if self.parameters:
            print("=== Testing Parameters ===")
            for i, param_value in enumerate(self.parameters):
                if i < len(self.param_names):
                    print(f"  {self.param_names[i]} = {param_value}")
            print()
    
    def measure_frequency(self, frequency):
        """
        Measure the RMS amplitude at a specific frequency for each output channel.
        
        Returns a list of RMS amplitudes (one per output channel), or None if measurement fails.
        """
        # Calculate frequency-dependent duration to capture at least 100 cycles
        # or minimum of 1 second for low frequencies
        min_cycles = 100
        cycle_duration = 1.0 / frequency
        adaptive_duration = max(min_cycles * cycle_duration, 1.0)
        
        # Cap maximum duration to avoid very long tests at low frequencies
        adaptive_duration = min(adaptive_duration, 10.0)
        
        # Adjust trim parameters proportionally
        trim_start = 0.2 * adaptive_duration / self.duration
        trim_len = 0.6 * adaptive_duration / self.duration
        
        # Create temporary files
        with tempfile.NamedTemporaryFile(suffix='.wav', delete=False) as tmp:
            tmp_input = tmp.name
        with tempfile.NamedTemporaryFile(suffix='.wav', delete=False) as tmp:
            tmp_output = tmp.name
        
        try:
            # Generate input signal with sox
            sox_cmd = [
                'sox', '-n', tmp_input,
                'synth', str(adaptive_duration), 'sine', str(frequency),
                'channels', str(self.num_inputs),
                'trim', str(trim_start), str(trim_len)
            ]
            
            result = subprocess.run(
                sox_cmd,
                capture_output=True,
                text=True
            )
            
            if result.returncode != 0:
                return None
            
            # Process through riaa_process instead of LADSPA
            # Check if we're testing the riaa plugin
            if self.plugin_label == 'riaa':
                # Use riaa_process native tool
                riaa_cmd = ['./riaa_process', tmp_input, tmp_output] + self.parameters
                result = subprocess.run(
                    riaa_cmd,
                    capture_output=True,
                    text=True
                )
                
                if result.returncode != 0:
                    return None
                    
                tmp_file = tmp_output
            else:
                # For other plugins, try using sox LADSPA
                cmd = [
                    'sox', tmp_input, tmp_output,
                    'ladspa', self.plugin_path, self.plugin_label
                ] + self.parameters
                
                result = subprocess.run(
                    cmd,
                    capture_output=True,
                    text=True
                )
                
                if result.returncode != 0:
                    return None
                    
                tmp_file = tmp_output
            
            # Now measure each channel individually
            rms_values = []
            for ch in range(self.num_outputs):
                # Extract single channel and measure RMS
                stat_cmd = [
                    'sox', tmp_file, '-n',
                    'remix', str(ch + 1),
                    'stat'
                ]
                
                stat_result = subprocess.run(
                    stat_cmd,
                    capture_output=True,
                    text=True
                )
                
                # Parse RMS amplitude from output
                output = stat_result.stdout + stat_result.stderr
                rms = None
                for line in output.split('\n'):
                    if 'RMS' in line and 'amplitude:' in line:
                        match = re.search(r'amplitude:\s+(\S+)', line)
                        if match:
                            rms = float(match.group(1))
                            break
                
                rms_values.append(rms if rms is not None else 0.0)
            
            return rms_values
            
        except (subprocess.CalledProcessError, ValueError) as e:
            print(f"Warning: Failed to measure at {frequency} Hz: {e}", file=sys.stderr)
            return None
        finally:
            # Clean up temporary files
            for f in [tmp_input, tmp_output]:
                try:
                    Path(f).unlink()
                except:
                    pass
    
    def rms_to_db(self, rms):
        """
        Convert RMS amplitude to dB.
        
        Applies correction for sine wave RMS = peak/sqrt(2)
        """
        if rms <= 0:
            return -200  # Very low value for silence
        
        # Convert to dB and correct for sine wave RMS offset
        # Sine wave RMS is peak/sqrt(2), so we add 20*log10(sqrt(2)) ≈ 3.0103 dB
        db = 20 * math.log10(rms) + 3.0103
        return db
    
    def run_sweep(self):
        """Run frequency resp or self.no_plotonse sweep."""
        print("=== Frequency Response Sweep ===")
        
        results = []
        frequency = self.f_min
        
        while frequency <= self.f_max:
            rms_values = self.measure_frequency(frequency)
            
            if rms_values is not None:
                # Convert each channel's RMS to dB
                db_values = [self.rms_to_db(rms) for rms in rms_values]
                results.append((frequency, db_values))
            else:
                # Use very low values for failed measurements
                db_values = [-200] * self.num_outputs
                results.append((frequency, db_values))
            
            # Calculate next frequency (logarithmic spacing)
            frequency *= 2 ** (1 / self.steps_per_oct)
        
        return results
    
    def write_results(self, results):
        """Write results to output file."""
        with open(self.output_file, 'w') as f:
            # Write header
            header = "Frequency"
            for ch in range(self.num_outputs):
                header += f"\tChannel_{ch}"
            f.write(header + "\n")
            
            # Write data
            for freq, db_values in results:
                line = f"{freq:.3f}"
                for db in db_values:
                    line += f"\t{db:.2f}"
                f.write(line + "\n")
        
        print(f"Wrote {len(results)} lines to {self.output_file}")
    
    def plot_results(self, results):
        """Generate frequency response plot."""
        if not HAS_MATPLOTLIB:
            return
        
        # Extract frequencies and dB values per channel
        frequencies = [r[0] for r in results]
        
        # Create figure
        plt.figure(figsize=(12, 6))
        
        # Define colors for up to 8 channels
        colors = ['blue', 'red', 'green', 'orange', 'purple', 'brown', 'pink', 'gray']
        
        # Plot each channel
        for ch in range(self.num_outputs):
            db_values = [r[1][ch] for r in results]
            color = colors[ch % len(colors)]
            label = f'Channel {ch}'
            plt.semilogx(frequencies, db_values, color=color, label=label, linewidth=1.5)
        
        plt.xlabel('Frequency (Hz)')
        plt.ylabel('Gain (dB)')
        plt.title(f'Frequency Response: {self.plugin_name}')
        plt.grid(True, which='both', alpha=0.3)
        plt.legend()
        
        # Set y-axis limits
        if self.y_min is not None and self.y_max is not None:
            # Use user-specified limits
            plt.ylim(self.y_min, self.y_max)
        else:
            # Auto-scale with 10% headroom
            all_db_values = []
            for ch in range(self.num_outputs):
                db_values = [r[1][ch] for r in results if r[1][ch] > -200]  # Exclude failed measurements
                all_db_values.extend(db_values)
            
            if all_db_values:
                data_min = min(all_db_values)
                data_max = max(all_db_values)
                range_db = data_max - data_min
                
                # Add 10% headroom on each side
                y_min = data_min - 0.1 * range_db if range_db > 0 else data_min - 1
                y_max = data_max + 0.1 * range_db if range_db > 0 else data_max + 1
                
                # Use user-specified min or max if provided
                y_min = self.y_min if self.y_min is not None else y_min
                y_max = self.y_max if self.y_max is not None else y_max
                
                plt.ylim(y_min, y_max)
        
        # Save plot
        plot_file = self.output_file.replace('.txt', '.png')
        plt.savefig(plot_file, dpi=150, bbox_inches='tight')
        print(f"Saved plot to {plot_file}")
    
    def run(self, args):
        """Main test execution."""
        plugin_name = self.parse_args(args)
        self.analyze_plugin()
        self.print_plugin_info()
        results = self.run_sweep()
        self.write_results(results)
        self.plot_results(results)


if __name__ == '__main__':
    tester = PluginTester()
    tester.run(sys.argv[1:])
