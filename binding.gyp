{
  'targets': [
    {
      'target_name': 'hci-btscan',
      'type': 'executable',
      'conditions': [
        ['OS=="linux"', {
          'sources': [
            'src/hci-btscan.c'
          ],
          'link_settings': {
            'libraries': [
              '-lbluetooth'
            ]
          }
        }]
      ]
    }
  ]
}
