# Urho3D Pure Depth SSAO shader

Implementation based on [this article](http://theorangeduck.com/page/pure-depth-ssao)

# Requirements
`ForwardDepth` renderpath should be used for it to work
```
viewport->SetRenderPath(cache->GetResource<XMLFile>("RenderPaths/ForwardDepth.xml"));
```

# Setup
Copy contents in the Urho3D source directory and build the engine as usual.

### Preview with strong SSAO settings
![alt tag](https://github.com/ArnisLielturks/Urho3D-SSAO-Shader/blob/master/Screenshots/preview.png)
