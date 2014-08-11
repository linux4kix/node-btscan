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
          'cflags': [
            '-I/usr/include',
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
