local changeset = {}

if slc.is_selected('ml_model') then
  return changeset
end

table.insert(changeset, {
  ['component'] = 'ml_model',
  ['action'] = 'add',
  ['status'] = 'user_verification',
  ['description'] = [[
Added ml_model. Edit the mlconf file for each model instance and set model parameter to your
.tflite file name/path (for example: model: blink.tflite). 
Saving the mlconf file will regenerate the project sourses.
]],
})

return changeset
