<?php

/**
 * @file
 * The admin sms functions.
 */

/**
 * Select default sms gateway form.
 */
function sms_admin_default_form() {
  $gateways = sms_gateways();

  foreach ($gateways as $identifier => $gateway) {
    $active = ($identifier == variable_get('sms_default_gateway', 'log'));
    $options[$identifier] = '';
    $form[$gateway['name']]['id'] = array('#value' => $identifier);
    if (function_exists($gateway['configure form'])) {
      $form[$gateway['name']]['configure'] = array('#value' => l(t('configure'), 'admin/smsframework/gateways/'. $identifier));
    }
    else {
      $form[$gateway['name']]['configure'] = array('#value' => t('No configuration options'));
    }
  }
  $form['default'] = array('#type' => 'radios', '#options' => $options, '#default_value' => variable_get('sms_default_gateway', 'log'));

  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Set default gateway'),
  );

  return $form;
}

/**
 * Form submission handler for sms_admin_default_form().
 *
 * @see sms_admin_default_form()
 */
function sms_admin_default_form_submit($form, &$form_state) {
  // Process form submission to set the default gateway
  if ($form_state['values']['default']) {
    drupal_set_message(t('Default gateway updated.'));
    variable_set('sms_default_gateway', $form_state['values']['default']);
  }
}

/**
 * Returns HTML for the admin default gateway form.
 *
 * @param $form
 *  An form array.
 *
 * @ingroup themeable
 */
function theme_sms_admin_default_form($form) {
  $rows = array();
  foreach ($form as $name => $element) {
    if (isset($element['id']) && is_array($element['id'])) {
      $rows[] = array(
        drupal_render($form['default'][$element['id']['#value']]),
        check_plain($name),
        drupal_render($element['configure']),
      );
      unset($form[$name]);
    }
  }
  $header = array(t('Default'), t('Name'), array('data' => t('Operations'), 'colspan' => 1));
  $output .= theme('table', $header, $rows);
  $output .= drupal_render($form);

  return $output;
}

/**
 * Form builder for the sms gateway configuration form.
 *
 * @param @gateway_id
 *  The ID of the sms gateway.
 *
 * @see sms_admin_gateway_form_validate()
 * @see sms_admin_gateway_form_submit()
 */
function sms_admin_gateway_form(&$form_state, $gateway_id) {
  $gateway = sms_gateways('gateway', $gateway_id);
  if ($gateway && function_exists($gateway['configure form'])) {
    drupal_set_title(t('@gateway configuration', array('@gateway' => $gateway['name'])));
    $form = $gateway['configure form']($gateway['configuration']);

    $form['submit'] = array(
      '#type' => 'submit',
      '#value' => t('Save'),
    );
    $form['gateway'] = array('#type' => 'value', '#value' => $gateway);

    return $form;
  }
  drupal_not_found();
  exit();
}

/**
 * Form validation handler for sms_admin_gateway_form().
 *
 * @see sms_admin_gateway_form()
 * @see sms_admin_gateway_form_submit()
 */
function sms_admin_gateway_form_validate($form, &$form_state) {
  // Pass validation to gateway
  $function = $form_state['values']['gateway']['configure form'] .'_validate';
  if (function_exists($function)) {
    $function($form, $form_state);
  }
}

/**
 * Form submission handler for sms_admin_gateway_form().
 *
 * @see sms_admin_gateway_form()
 * @see sms_admin_gateway_form_validate()
 */
function sms_admin_gateway_form_submit($form, &$form_state) {
  $gateway = $form_state['values']['gateway'];

  // Remove uneccesary values
  unset($form_state['values']['op'], $form_state['values']['submit'], $form_state['values']['gateway'], $form_state['values']['form_token'], $form_state['values']['form_id']);
  variable_set('sms_'. $gateway['identifier'] .'_settings', $form_state['values']);
  drupal_set_message(t('The gateway settings have been saved.'));
  $form_state['redirect'] = 'admin/smsframework/gateways';
}

/**
 * Form builder for the list of sms carriers.
 *
 * @see sms_carriers_admin_form_submit().
 */
function sms_carriers_admin_form() {
  $carriers = sms_carriers();

  $form = array();
  foreach ($carriers as $id => $carrier) {
    $actions = array();
    $css_safe_id = str_replace('.', '-', $id);

    switch ($carrier['type']) {
      case SMS_CARRIER_DEFAULT:
        $storage = t('Default');
        break;
      case SMS_CARRIER_OVERRIDDEN:
        $storage = t('Overridden');
        break;
      case SMS_CARRIER_NORMAL:
        $storage = t('Normal');
        break;
    }
    $form['status']['#tree'] = TRUE;
    $form['status'][$css_safe_id] = array(
      '#type' => 'checkbox',
      '#title' => $carrier['name'],
      '#description' => $storage,
      '#default_value' => $carrier['status'],
    );
    $form['prefix'][$css_safe_id] = array(
      '#type' => 'markup',
      '#value' => $carrier['prefix']
    );
    $form['receive'][$css_safe_id] = array(
      '#type' => 'markup',
      '#value' => $carrier['receive']
    );

    $form['domain'][$css_safe_id] = array(
      '#type' => 'markup',
      '#value' => $id,
    );

    $actions[] = l(t('Edit'), "admin/smsframework/carriers/{$id}");

    if ($carrier['type'] == SMS_CARRIER_OVERRIDDEN) {
      $actions[] = l(t('Revert'), "admin/smsframework/carriers/delete/{$id}");
    }
    else if ($carrier['type'] == SMS_CARRIER_NORMAL) {
      $actions[] = l(t('Delete'), "admin/smsframework/carriers/delete/{$id}");
    }

    $form['actions'][$css_safe_id] = array(
      '#type' => 'markup',
      '#value' => implode(' | ', $actions),
    );
  }

  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save settings'),
  );

  return $form;
}

/**
 * Form submission handler for sms_carriers_admin_form().
 *
 * @see sms_carriers_admin_form().
 */
function sms_carriers_admin_form_submit(&$form, &$form_state) {
  $enabled_carriers = array();
  foreach ($form_state['values']['status'] as $carrier => $status) {
    if ($status) {
      $enabled_carriers[] = str_replace('-', '.', $carrier);
    }
  }
  variable_set('sms_enabled_carriers', $enabled_carriers);
  drupal_set_message('The configuration options have been saved.');
}

/**
 * Form builder for the carrier edit form.
 *
 * @param $carrier
 *   An associative array defining the sms carrier.
 *
 * @see sms_carriers_edit_form_submit().
 */
function sms_carriers_edit_form($form_state, $carrier = array()) {
  $form['carrier'] = array(
    '#type' => 'value',
    '#value' => $carrier['domain'],
  );

  $form['name'] = array(
    '#type' => 'textfield',
    '#title' => t('Name'),
    '#default_value' => $carrier['name'],
    '#required' => TRUE,
  );

  $form['domain'] = array(
    '#type' => 'textfield',
    '#title' => t('Domain'),
    '#default_value' => $carrier['domain'],
    '#required' => TRUE,
  );
  
  $form['prefix'] = array(
    '#type' => 'textfield',
    '#title' => t('Prefix'),
    '#default_value' => $carrier['prefix']
  );

  $form['receive'] = array(
    '#type' => 'checkbox',
    '#title' => t('Can Receive?'),
    '#default_value' => $carrier['receive']
  );

  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save'),
  );

  return $form;
}

/**
 * Form submission handler for sms_carriers_edit_form().
 *
 * @see sms_carriers_edit_form().
 */
function sms_carriers_edit_form_submit(&$form, &$form_state) {
  $carrier = array();
  $carrier = array(
    'name' => $form_state['values']['name'],
    'domain' => $form_state['values']['domain'],
    'prefix' => $form_state['values']['prefix'],
    'receive' => $form_state['values']['receive']
  );
  carrier_save($form_state['values']['carrier'], $carrier);
  drupal_set_message(t('The carrier has been saved.'));
  drupal_goto('admin/smsframework/carriers');
}

/**
 * Form builder for the carrier deletion confirmation form.
 *
 * @param $carrier
 *   An associative array defining the sms carrier.
 *
 * @see sms_carriers_delete_form_submit()
 */
function sms_carriers_delete_form(&$form_state, $carrier) {
  $form['domain'] = array('#type' => 'value', '#value' => $carrier['domain']);
  $form['type'] = array('#type' => 'value', '#value' => $carrier['type']);
  if ($carrier['type'] == SMS_CARRIER_OVERRIDDEN) {
    return confirm_form($form, t('Are you sure you want revert %carrier?', array('%carrier' => $carrier['name'])), 'admin/smsframework/carriers', t('Reverting this carrier will delete it from the database. It will be replaced with the default carrier settings. This action cannot be undone.'), t('Revert'), t('Cancel'));
  }
  if ($carrier['type'] == SMS_CARRIER_NORMAL) {
    return confirm_form($form, t('Are you sure you want delete %carrier?', array('%carrier' => $carrier['name'])), 'admin/smsframework/carriers', t('This carrier will be removed from the database. This action cannot be undone.'), t('Delete'), t('Cancel'));
  }
}

/**
 * Form submission handler for sms_carriers_delete_form().
 *
 * @see sms_carriers_delete_form()
 */
function sms_carriers_delete_form_submit($form, &$form_state) {
  db_query('DELETE FROM {sms_carriers} WHERE domain = "%s"', $form_state['values']['domain']);
  if ($carrier['type'] == SMS_CARRIER_OVERRIDDEN) {
    drupal_set_message(t('The carrier has been reverted.'));
  }
  if ($carrier['type'] == SMS_CARRIER_NORMAL) {
    drupal_set_message(t('The carrier has been deleted.'));
  }

  $form_state['redirect'] = 'admin/smsframework/carriers';
}

/**
 * Returns HTML for the sms carriers admin form.
 *
 * @param $form
 *  A form array.
 *
 * @ingroup themeable
 */
function theme_sms_carriers_admin_form($form) {
  $output = '';

  $header = array('', t('Carrier'), t('Domain'), t('Prefix'), t('Can Receive?'), t('Actions'));

  $rows = array();
  foreach (element_children($form['status']) as $element) {
    $name = "<div class='carrier'>";
    $name .= "<strong>{$form['status'][$element]['#title']}</strong>";
    $name .= "<div class='description'>{$form['status'][$element]['#description']}</div>";
    $name .= "</div>";
    unset($form['status'][$element]['#title']);
    unset($form['status'][$element]['#description']);

    $row = array(
      'status' => drupal_render($form['status'][$element]),
      'name' => $name,
      'domain' => drupal_render($form['domain'][$element]),
      'prefix' => drupal_render($form['prefix'][$element]),
      'receive' => drupal_render($form['receive'][$element]) ? 'Yes' : 'No',
      'actions' =>  drupal_render($form['actions'][$element]),
    );
    $rows[] = $row;
  }

  $output .= theme('table', $header, $rows, array('id' => 'sms-form-table', 'class' => 'sms'));
  $output .= drupal_render($form['submit']);
  $output .= drupal_render($form);

  return $output;
}
