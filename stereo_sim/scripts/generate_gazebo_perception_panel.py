#!/usr/bin/env python3
"""Create a user-local Gazebo Classic model from one synthetic panel case."""

import argparse
import json
from pathlib import Path
import xml.etree.ElementTree as ET

import cv2
from PIL import ImageFont

from generate_panel_perception_cases import CASES, make_case, marker_image


MODEL_CONFIG = '''<?xml version="1.0"?>
<model>
  <name>perception_panel</name>
  <version>1.0</version>
  <sdf version="1.7">model.sdf</sdf>
  <description>Generated synthetic Chinese fire-panel camera target.</description>
</model>
'''

MODEL_SDF = '''<?xml version="1.0"?>
<sdf version="1.7">
  <model name="perception_panel">
    <static>true</static>
    <link name="body">
      <visual name="backing">
        <geometry><box><size>0.020 0.960 0.540</size></box></geometry>
        <material><ambient>0.6 0.6 0.6 1</ambient></material>
      </visual>
      <visual name="textured_face">
        <geometry>
          <mesh><uri>model://perception_panel/meshes/panel.dae</uri></mesh>
        </geometry>
        <material>
          <script>
            <uri>model://perception_panel/materials/scripts</uri>
            <uri>model://perception_panel/materials/textures</uri>
            <name>Perception/FirePanel</name>
          </script>
          <lighting>false</lighting>
        </material>
      </visual>
      <collision name="backing_collision">
        <geometry><box><size>0.020 0.960 0.540</size></box></geometry>
      </collision>
    </link>
  </model>
</sdf>
'''

PANEL_DAE = '''<?xml version="1.0" encoding="utf-8"?>
<COLLADA xmlns="http://www.collada.org/2005/11/COLLADASchema" version="1.4.1">
  <asset><contributor><authoring_tool>stereo_sim synthetic panel generator</authoring_tool></contributor>
    <unit name="meter" meter="1"/><up_axis>Z_UP</up_axis></asset>
  <library_images><image id="panel-image" name="panel-image"><init_from>../materials/textures/panel.png</init_from></image></library_images>
  <library_effects><effect id="panel-effect"><profile_COMMON>
    <newparam sid="panel-surface"><surface type="2D"><init_from>panel-image</init_from></surface></newparam>
    <newparam sid="panel-sampler"><sampler2D><source>panel-surface</source></sampler2D></newparam>
    <technique sid="common"><phong>
      <emission><texture texture="panel-sampler" texcoord="UVMap"/></emission>
      <ambient><color>0.25 0.25 0.25 1</color></ambient>
      <diffuse><texture texture="panel-sampler" texcoord="UVMap"/></diffuse>
      <specular><color>0 0 0 1</color></specular><shininess><float>0</float></shininess>
    </phong></technique>
  </profile_COMMON></effect></library_effects>
  <library_materials><material id="panel-material" name="panel-material"><instance_effect url="#panel-effect"/></material></library_materials>
  <library_geometries><geometry id="panel-geometry" name="panel-geometry"><mesh>
    <source id="panel-positions"><float_array id="panel-positions-array" count="12">-0.012 0.480 0.270 -0.012 -0.480 0.270 -0.012 -0.480 -0.270 -0.012 0.480 -0.270</float_array>
      <technique_common><accessor source="#panel-positions-array" count="4" stride="3"><param name="X" type="float"/><param name="Y" type="float"/><param name="Z" type="float"/></accessor></technique_common></source>
    <source id="panel-normals"><float_array id="panel-normals-array" count="3">-1 0 0</float_array>
      <technique_common><accessor source="#panel-normals-array" count="1" stride="3"><param name="X" type="float"/><param name="Y" type="float"/><param name="Z" type="float"/></accessor></technique_common></source>
    <source id="panel-uv"><float_array id="panel-uv-array" count="8">0 1 1 1 1 0 0 0</float_array>
      <technique_common><accessor source="#panel-uv-array" count="4" stride="2"><param name="S" type="float"/><param name="T" type="float"/></accessor></technique_common></source>
    <vertices id="panel-vertices"><input semantic="POSITION" source="#panel-positions"/></vertices>
    <triangles material="PanelMaterial" count="2"><input semantic="VERTEX" source="#panel-vertices" offset="0"/><input semantic="NORMAL" source="#panel-normals" offset="1"/><input semantic="TEXCOORD" source="#panel-uv" offset="2" set="0"/>
      <p>0 0 0 2 0 2 1 0 1 0 0 0 3 0 3 2 0 2</p>
    </triangles>
  </mesh></geometry></library_geometries>
  <library_visual_scenes><visual_scene id="Scene" name="Scene"><node id="panel" name="panel">
    <instance_geometry url="#panel-geometry"><bind_material><technique_common><instance_material symbol="PanelMaterial" target="#panel-material"><bind_vertex_input semantic="UVMap" input_semantic="TEXCOORD" input_set="0"/></instance_material></technique_common></bind_material></instance_geometry>
  </node></visual_scene></library_visual_scenes>
  <scene><instance_visual_scene url="#Scene"/></scene>
</COLLADA>
'''

MATERIAL = '''material Perception/FirePanel
{
  technique
  {
    pass
    {
      lighting off
      texture_unit
      {
        texture panel.png
      }
    }
  }
}
'''


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--case', choices=[item[0] for item in CASES],
                        default='fire_on')
    parser.add_argument('--font', type=Path, default=Path(
        '/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf'))
    args = parser.parse_args()
    name, states, variant = next(item for item in CASES if item[0] == args.case)
    model = args.output / 'models' / 'perception_panel'
    textures = model / 'materials' / 'textures'
    meshes = model / 'meshes'
    scripts = model / 'materials' / 'scripts'
    textures.mkdir(parents=True, exist_ok=True)
    meshes.mkdir(parents=True, exist_ok=True)
    scripts.mkdir(parents=True, exist_ok=True)
    image, truth = make_case(states, variant,
                             ImageFont.truetype(str(args.font), 58),
                             marker_image())
    cv2.imwrite(str(textures / 'panel.png'),
                cv2.cvtColor(image, cv2.COLOR_RGB2BGR))
    (model / 'model.config').write_text(MODEL_CONFIG, encoding='utf-8')
    (model / 'model.sdf').write_text(MODEL_SDF, encoding='utf-8')
    (meshes / 'panel.dae').write_text(PANEL_DAE, encoding='utf-8')
    (scripts / 'panel.material').write_text(MATERIAL, encoding='utf-8')
    ET.fromstring(MODEL_CONFIG)
    ET.fromstring(MODEL_SDF)
    ET.parse(meshes / 'panel.dae')
    metadata = {'case': name, 'domain': 'synthetic_only',
                'model': 'perception_panel', 'image': 'models/perception_panel/materials/textures/panel.png',
                'pixel_to_meter': 0.001, 'truth': truth}
    (args.output / 'generation.json').write_text(
        json.dumps(metadata, ensure_ascii=False, indent=2) + '\n',
        encoding='utf-8')
    print(f'Generated {name} textured Gazebo model at {model}')


if __name__ == '__main__':
    main()
