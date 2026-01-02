#!/usr/bin/env node
/**
 * check-macos-abi.js - Detect macOS ABI mismatches before build
 *
 * Compares MACOSX_DEPLOYMENT_TARGET in binding.gyp against FFmpeg's
 * minimum OS version. ABI mismatches cause cryptic segfaults in STL
 * types (std::function, std::vector) that are nearly impossible to debug.
 */

const { execSync } = require('child_process');
const fs = require('fs');
const path = require('path');

if (process.platform !== 'darwin') {
  process.exit(0);
}

function getFFmpegMinOS() {
  try {
    const libdir = execSync('pkg-config --variable=libdir libavcodec 2>/dev/null', {
      encoding: 'utf8',
    }).trim();

    const otoolOutput = execSync(`otool -l "${libdir}/libavcodec.dylib" 2>/dev/null`, {
      encoding: 'utf8',
    });

    // Parse LC_BUILD_VERSION to find minos
    const match = otoolOutput.match(/LC_BUILD_VERSION[\s\S]*?minos\s+(\d+\.\d+)/);
    if (match) {
      return match[1];
    }

    // Fallback: check LC_VERSION_MIN_MACOSX
    const minMatch = otoolOutput.match(/LC_VERSION_MIN_MACOSX[\s\S]*?version\s+(\d+\.\d+)/);
    if (minMatch) {
      return minMatch[1];
    }

    return null;
  } catch {
    return null;
  }
}

function getBindingGyp() {
  const bindingPath = path.join(__dirname, '..', 'binding.gyp');
  try {
    const content = fs.readFileSync(bindingPath, 'utf8');
    const match = content.match(/MACOSX_DEPLOYMENT_TARGET["']?\s*:\s*["'](\d+\.?\d*)["']/);
    return match ? match[1] : null;
  } catch {
    return null;
  }
}

function parseVersion(v) {
  const parts = v.split('.').map(Number);
  return parts[0] * 100 + (parts[1] || 0);
}

function main() {
  const ffmpegMinOS = getFFmpegMinOS();
  const deploymentTarget = getBindingGyp();

  if (!ffmpegMinOS) {
    console.log('⚠️  Could not detect FFmpeg minimum macOS version (skipping ABI check)');
    process.exit(0);
  }

  if (!deploymentTarget) {
    console.log('⚠️  Could not read MACOSX_DEPLOYMENT_TARGET from binding.gyp');
    process.exit(0);
  }

  const ffmpegVer = parseVersion(ffmpegMinOS);
  const targetVer = parseVersion(deploymentTarget);

  console.log(`FFmpeg libavcodec minos:     ${ffmpegMinOS}`);
  console.log(`binding.gyp deployment target: ${deploymentTarget}`);

  if (targetVer < ffmpegVer) {
    console.error(`
╔════════════════════════════════════════════════════════════════════╗
║  ❌ ABI MISMATCH DETECTED                                          ║
╠════════════════════════════════════════════════════════════════════╣
║  FFmpeg was built for macOS ${ffmpegMinOS.padEnd(4)}, but binding.gyp targets ${deploymentTarget.padEnd(5)}  ║
║                                                                    ║
║  This causes segfaults in STL types (std::function, std::vector)   ║
║  that appear as crashes during object instantiation.               ║
║                                                                    ║
║  FIX: Update binding.gyp MACOSX_DEPLOYMENT_TARGET to "${ffmpegMinOS}"        ║
╚════════════════════════════════════════════════════════════════════╝
`);
    process.exit(1);
  }

  console.log('✅ macOS ABI versions compatible');
}

main();
